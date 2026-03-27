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

static void ScanSPIRAM(struct ExecBase* SysBase)
{
    volatile UWORD* base = (volatile UWORD*)0x620000;

    UWORD saved = *base;
    *base = 0x55AA;
    if (*base != 0x55AA) { kprintf("SPIRAM: probe failed\n"); return; }
    *base = 0xAA55;
    if (*base != 0xAA55) { kprintf("SPIRAM: probe failed\n"); *base = saved; return; }
    *base = saved;

    kprintf("SPIRAM: adding 0x620000-0x720000 (1024 KB)\n");
    AddMemList(0x3d0000, MEMF_PUBLIC | MEMF_FAST, 0, (APTR)0x620000, (STRPTR)"PCMCIA SRAM");

    *(volatile uint8_t *)0xA20206 = 0x01;


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
