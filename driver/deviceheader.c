#include <proto/exec.h>
#include <proto/expansion.h>
#include <exec/resident.h>
#include <exec/alerts.h>
#include <exec/memory.h>
#include <libraries/expansionbase.h>
#include <dos/filehandler.h>

#include "device.h"
#include "spi.h"
#include "rdb.h"
#include "debug.h"


// Inline NewList to avoid needing alib
static inline void InitList(struct List* list)
{
    list->lh_Head = (struct Node*)&list->lh_Tail;
    list->lh_Tail = NULL;
    list->lh_TailPred = (struct Node*)&list->lh_Head;
}

// Dummy entry point — return error if executed as a program
int NotExeDummy(void) { return -1; }

static struct DevBase* Init(
    register ULONG libraryBase __asm("d0"),
    register BPTR segList __asm("a0"),
    register struct ExecBase* SysBase __asm("a6")
);

extern const char _end;

const char DeviceName[] = "tfpcmcia.device";
const char DeviceIDString[] = "tfpcmcia 1.0 (08.03.2026)";

__attribute__((section(".text"))) const struct Resident RomTag =
{
    RTC_MATCHWORD,
    (struct Resident*)&RomTag,
    (struct Resident*)&_end,
    RTF_COLDSTART,
    DEVICE_VERSION,
    NT_DEVICE,
    DEVICE_PRIORITY,
    (char*)DeviceName,
    (char*)DeviceIDString,
    (APTR)Init
};

// Function table for MakeLibrary
static const APTR LibFunctions[] =
{
    (APTR)DevOpen,
    (APTR)DevClose,
    (APTR)DevExpunge,
    (APTR)DevNull,
    (APTR)DevBeginIO,
    (APTR)DevAbortIO,
    (APTR)-1
};

// I/O task entry wrapper
static void IOTaskEntry(void)
{
    IOTask();
}

// Card RAM handed to Exec: the whole common-memory window above the 128 KB
// boot ROM ($600000-$61FFFF). The card serves every read from 0x20000 up,
// and every write, from its PSRAM.
#define SPIRAM_BASE       0x620000UL
#define SPIRAM_SIZE       0x3E0000UL  // $620000-$9FFFFF, 3968 KB

// Set to 0 to leave the card RAM out of the free list (no fast RAM).
#ifndef SPIRAM_ADD
#define SPIRAM_ADD 1
#endif

// Boot-time test of the card RAM before it is added to the free list.
#ifndef SPIRAM_MEMTEST
#define SPIRAM_MEMTEST 0
#endif

#define SPIRAM_BYTE_SIZE  0x10000UL   // byte-lane test covers the first 64 KB
#define MEMTEST_MAX_REPORT 4          // failures printed per test

#if SPIRAM_MEMTEST
static void MemTestFail(const char* test, ULONG* count, ULONG addr, ULONG expect, ULONG got)
{
    if (*count < MEMTEST_MAX_REPORT)
        kprintf("MEMTEST %s: %08lx wrote %08lx read %08lx (xor %08lx)\n",
                test, addr, expect, got, expect ^ got);
    (*count)++;
}

static inline ULONG LongPattern(ULONG addr)
{
    return addr ^ (addr << 13) ^ 0x5A3C96E1UL;
}

// Returns the number of errors found in the card RAM.
static ULONG MemTestSPIRAM(void)
{
    volatile ULONG* l = (volatile ULONG*)SPIRAM_BASE;
    volatile UWORD* w = (volatile UWORD*)SPIRAM_BASE;
    volatile UBYTE* b = (volatile UBYTE*)SPIRAM_BASE;
    ULONG a;
    ULONG err_movel = 0, err_word = 0, err_even = 0, err_odd = 0;

    kprintf("MEMTEST: %08lx-%08lx\n", SPIRAM_BASE, SPIRAM_BASE + SPIRAM_SIZE - 1);

    // 1. Address-derived longs, read back with move.l (on the 68000 two
    //    back-to-back word cycles): address aliasing, data bits.
    for (a = 0; a < SPIRAM_SIZE; a += 4)
        l[a >> 2] = LongPattern(SPIRAM_BASE + a);
    for (a = 0; a < SPIRAM_SIZE; a += 4)
    {
        ULONG e = LongPattern(SPIRAM_BASE + a);
        ULONG g = l[a >> 2];
        if (g != e) MemTestFail("move.l", &err_movel, SPIRAM_BASE + a, e, g);
    }
    kprintf("MEMTEST move.l: %ld errors\n", err_movel);

    // 2. Same pattern inverted, read back as two word reads: every bit seen
    //    in both states, single 16-bit read cycles.
    for (a = 0; a < SPIRAM_SIZE; a += 4)
        l[a >> 2] = ~LongPattern(SPIRAM_BASE + a);
    for (a = 0; a < SPIRAM_SIZE; a += 4)
    {
        ULONG e = ~LongPattern(SPIRAM_BASE + a);
        ULONG hi = w[a >> 1];
        ULONG lo = w[(a >> 1) + 1];
        ULONG g = (hi << 16) | lo;
        if (g != e) MemTestFail("word", &err_word, SPIRAM_BASE + a, e, g);
    }
    kprintf("MEMTEST word: %ld errors\n", err_word);

    // 3. Byte lanes: write one byte of each word, check the word (the other
    //    byte must survive the card's read-modify-write).
    for (a = 0; a < SPIRAM_BYTE_SIZE; a += 2)
    {
        w[a >> 1] = 0xA55A;
        b[a] = (UBYTE)(a >> 1);                 // even (UDS) byte
        UWORD e = (UWORD)(((a >> 1) & 0xFF) << 8) | 0x5A;
        UWORD g = w[a >> 1];
        if (g != e) MemTestFail("byte-even", &err_even, SPIRAM_BASE + a, e, g);

        w[a >> 1] = 0xA55A;
        b[a + 1] = (UBYTE)(a >> 1);             // odd (LDS) byte
        e = 0xA500 | ((a >> 1) & 0xFF);
        g = w[a >> 1];
        if (g != e) MemTestFail("byte-odd", &err_odd, SPIRAM_BASE + a, e, g);
    }
    kprintf("MEMTEST byte: %ld even / %ld odd errors\n", err_even, err_odd);

    return err_movel + err_word + err_even + err_odd;
}
#endif

// Diagnostic: sample Gayle's card status register and report every distinct
// value seen, plus the interrupt request/enable registers. Bits (cardstatus):
// 40 CCDET, 20 BVD1/SC, 10 BVD2/DA, 08 WR(WP), 04 BSY/IRQ(READY).
#ifndef GAYLE_DIAG
#define GAYLE_DIAG 0
#endif

#if GAYLE_DIAG
static void GayleDiag(void)
{
    volatile UBYTE* cs     = (volatile UBYTE*)0xDA8000;
    volatile UBYTE* intreq = (volatile UBYTE*)0xDA9000;
    volatile UBYTE* inten  = (volatile UBYTE*)0xDAA000;
    UBYTE seen[8];
    ULONG count[8];
    ULONG n = 0, i, j;

    kprintf("GAYLE: cs=%02lx intreq=%02lx inten=%02lx\n",
            (ULONG)*cs, (ULONG)*intreq, (ULONG)*inten);
    for (i = 0; i < 50000; i++)
    {
        UBYTE v = *cs;
        for (j = 0; j < n && seen[j] != v; j++) ;
        if (j == n && n < 8) { seen[n] = v; count[n] = 0; n++; }
        if (j < n) count[j]++;
    }
    for (j = 0; j < n; j++)
        kprintf("GAYLE: cs value %02lx seen %ld/50000\n", (ULONG)seen[j], count[j]);
    kprintf("GAYLE: after sampling intreq=%02lx\n", (ULONG)*intreq);
}

// Diagnostic: the cold-boot Guru always names Task $7312. Chip RAM survives
// the alert's warm reboot, so on the next boot dump what is left there,
// plus Exec's record of the last alert.
static int Printable(const UBYTE* s)
{
    ULONG i;
    for (i = 0; i < 40 && s[i]; i++)
        if (s[i] < 0x20 || s[i] > 0x7E) return 0;
    return i > 0 && i < 40;
}

static void LastAlertDiag(struct ExecBase* SysBase)
{
    const UBYTE* t = (const UBYTE*)0x7312;
    ULONG name = *(const ULONG*)(t + 10);
    ULONG i;

    kprintf("ALERT: LastAlert %08lx %08lx %08lx %08lx\n",
            SysBase->LastAlert[0], SysBase->LastAlert[1],
            SysBase->LastAlert[2], SysBase->LastAlert[3]);
    kprintf("ALERT: $7312 type=%02lx pri=%ld name=%08lx state=%02lx sp=%08lx splower=%08lx spupper=%08lx\n",
            (ULONG)t[8], (LONG)(BYTE)t[9], name, (ULONG)t[15],
            *(const ULONG*)(t + 54), *(const ULONG*)(t + 58), *(const ULONG*)(t + 62));
    if ((name < 0x200000 || (name >= 0xF80000 && name < 0x1000000)) && Printable((const UBYTE*)name))
        kprintf("ALERT: $7312 name '%s'\n", (const char*)name);
    for (i = 0; i < 0x50; i += 16)
        kprintf("ALERT: %08lx: %08lx %08lx %08lx %08lx\n", (ULONG)(t + i),
                *(const ULONG*)(t + i), *(const ULONG*)(t + i + 4),
                *(const ULONG*)(t + i + 8), *(const ULONG*)(t + i + 12));
}
#endif

static void ScanSPIRAM(struct ExecBase* SysBase)
{
    volatile UWORD* base = (volatile UWORD*)SPIRAM_BASE;

    UWORD saved = *base;
    *base = 0x55AA;
    if (*base != 0x55AA) { kprintf("SPIRAM: probe failed\n"); return; }
    *base = 0xAA55;
    if (*base != 0xAA55) { kprintf("SPIRAM: probe failed\n"); *base = saved; return; }
    *base = saved;

#if SPIRAM_MEMTEST
    if (MemTestSPIRAM())
    {
        kprintf("SPIRAM: memory test failed, not adding\n");
        return;
    }
#endif

#if !SPIRAM_ADD
    kprintf("SPIRAM: not added (SPIRAM_ADD=0)\n");
    return;
#endif

    kprintf("SPIRAM: adding %08lx-%08lx (%ld KB)\n",
            SPIRAM_BASE, SPIRAM_BASE + SPIRAM_SIZE - 1, SPIRAM_SIZE >> 10);
    AddMemList(SPIRAM_SIZE, MEMF_PUBLIC | MEMF_FAST, 0, (APTR)SPIRAM_BASE, (STRPTR)"PCMCIA SRAM");
}

static struct DevBase* Init(
    register ULONG libraryBase __asm("d0"),
    register BPTR segList __asm("a0"),
    register struct ExecBase* SysBase __asm("a6")
)
{
    kprintf("\nInit '%s':\n  %s\n", RomTag.rt_Name, RomTag.rt_IdString);

    struct DevBase* db = (struct DevBase*)MakeLibrary(
        (APTR)LibFunctions, NULL, NULL, sizeof(struct DevBase), 0
    );
    if (!db)
    {
        kprintf("MakeLibrary failed!\n");
        Alert(AN_TrackDiskDev | AG_NoMemory);
        return NULL;
    }

    struct Library* lib = &db->db_Device.dd_Library;
    lib->lib_Node.ln_Type = NT_DEVICE;
    lib->lib_Node.ln_Name = (char*)DeviceName;
    lib->lib_Flags = LIBF_SUMUSED | LIBF_CHANGED;
    lib->lib_Version = DEVICE_VERSION;
    lib->lib_Revision = DEVICE_REVISION;
    lib->lib_IdString = (char*)DeviceIDString;

    db->db_SysBase = SysBase;
    db->db_SegList = segList;

    // Init SD card
    SdSetSysBase(SysBase);
    UBYTE cardType = SdInit();
    db->db_CardType = cardType;
    if (cardType == SD_TYPE_NONE)
    {
        kprintf("SD card init failed\n");
    }
    else
    {
        kprintf("SD card type: %ld\n", (ULONG)cardType);
    }

    // Create I/O task with message port
    struct TaskBlock
    {
        struct Task task;
        struct MsgPort msgPort;
        ULONG stack[DEVSTACK / sizeof(ULONG)];
    };

    struct TaskBlock* tb = AllocMem(sizeof(struct TaskBlock), MEMF_PUBLIC | MEMF_CLEAR);
    if (!tb)
    {
        kprintf("Failed to alloc IOTask!\n");
        Alert(AN_TrackDiskDev | AG_NoMemory);
        return NULL;
    }

    struct Task* task = &tb->task;
    struct MsgPort* port = &tb->msgPort;

    // Init message port — store parent task temporarily in mp_SigTask
    port->mp_Node.ln_Name = (char*)DeviceName;
    port->mp_Node.ln_Type = NT_MSGPORT;
    port->mp_SigTask = FindTask(NULL);
    InitList(&port->mp_MsgList);

    db->db_IOPort = port;

    // Set up task
    task->tc_SPLower = &tb->stack[0];
    task->tc_SPUpper = &tb->stack[DEVSTACK / sizeof(ULONG)];
    task->tc_SPReg = task->tc_SPUpper;
    task->tc_Node.ln_Type = NT_TASK;
    task->tc_Node.ln_Pri = DEVICE_PRIORITY + 1;
    task->tc_Node.ln_Name = (char*)DeviceName;
    task->tc_UserData = db;

    SetSignal(0, SIGF_SINGLE);
    AddTask(task, IOTaskEntry, 0);

    kprintf("Waiting for IOTask...\n");
    Wait(SIGF_SINGLE);
    kprintf("IOTask running\n");

    db->db_IOTask = task;

    // Register device
    AddDevice((struct Device*)db);

#if GAYLE_DIAG
    LastAlertDiag(SysBase);
    GayleDiag();
#endif
    ScanSPIRAM(SysBase);

    // Mount partitions from RDB
    if (db->db_CardType != SD_TYPE_NONE)
    {
        MountUnit(db);

        // Remove all non-tfpcmcia BootNodes from the MountList so DOS
        // only sees our partitions (avoids IDE timeouts and conflicting mounts)
        {
            struct Library* expBase = OpenLibrary((CONST_STRPTR)"expansion.library", 0);
            if (expBase)
            {
                struct ExpansionBase* eb = (struct ExpansionBase*)expBase;
                struct BootNode* bn = (struct BootNode*)eb->MountList.lh_Head;
                kprintf("MountList:\n");
                while (bn->bn_Node.ln_Succ != NULL)
                {
                    struct BootNode* next = (struct BootNode*)bn->bn_Node.ln_Succ;

                    // Get DOS device name from DeviceNode's dn_Name (BSTR)
                    char dosName[32];
                    dosName[0] = '?';
                    dosName[1] = '\0';
                    struct DeviceNode* dn = (struct DeviceNode*)bn->bn_DeviceNode;
                    if (dn && dn->dn_Name)
                    {
                        UBYTE* bstr = (UBYTE*)BADDR(dn->dn_Name);
                        UBYTE len = bstr[0];
                        if (len > sizeof(dosName) - 2)
                            len = sizeof(dosName) - 2;
                        for (UBYTE j = 0; j < len; j++)
                            dosName[j] = bstr[j + 1];
                        dosName[len] = ':';
                        dosName[len + 1] = '\0';
                    }

                    // Check if this BootNode belongs to us by matching the
                    // device driver name in FileSysStartupMsg
                    int ours = 0;
                    if (dn && dn->dn_Startup)
                    {
                        struct FileSysStartupMsg* fssm =
                            (struct FileSysStartupMsg*)BADDR(dn->dn_Startup);
                        if (fssm && fssm->fssm_Device)
                        {
                            UBYTE* devBstr = (UBYTE*)BADDR(fssm->fssm_Device);
                            UBYTE devLen = devBstr[0];
                            kprintf("    fssm=%08lx dev=%08lx len=%ld '",
                                (ULONG)fssm, (ULONG)BADDR(fssm->fssm_Device), (ULONG)devLen);
                            for (UBYTE j = 0; j < devLen && j < 30; j++)
                                kprintf("%lc", (ULONG)devBstr[j + 1]);
                            kprintf("'\n");
                            // Compare with "tfpcmcia.device"
                            // BSTR length may include trailing null
                            UBYTE cmpLen = sizeof(DeviceName) - 1;
                            if (devLen == cmpLen || devLen == cmpLen + 1)
                            {
                                int match = 1;
                                for (UBYTE j = 0; j < cmpLen; j++)
                                {
                                    if (devBstr[j + 1] != DeviceName[j])
                                    {
                                        match = 0;
                                        break;
                                    }
                                }
                                ours = match;
                            }
                        }
                    }

                    if (ours)
                    {
                        kprintf("  keep %s\n", dosName);
                    }
                    else
                    {
                        kprintf("  remove %s\n", dosName);
                        Remove((struct Node*)bn);
                    }

                    bn = next;
                }
                CloseLibrary(expBase);
            }
        }

        // PCMCIA XIP runs after the strap module has already scanned the
        // MountList, so our BootNodes will never be picked up by the strap's
        // polling loop.  Start DOS directly, same as a standard boot block.
        struct Resident* dosRes = FindResident((CONST_STRPTR)"dos.library");
        if (dosRes)
        {
            kprintf("Starting DOS via InitResident(%08lx)\n", (ULONG)dosRes);
            InitResident(dosRes, 0);
        }
        else
        {
            kprintf("dos.library resident not found!\n");
        }
    }

    kprintf("Init done\n");
    return db;
}
