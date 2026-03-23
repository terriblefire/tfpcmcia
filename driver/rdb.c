#include <proto/exec.h>
#include <proto/expansion.h>
#include <libraries/expansion.h>
#include <libraries/expansionbase.h>
#include <exec/memory.h>
#include <exec/errors.h>
#include <resources/filesysres.h>
#include <devices/hardblocks.h>
#include <devices/trackdisk.h>

#include "device.h"
#include "rdb.h"
#include "spi.h"
#include "lseg.h"
#include "debug.h"

// Read a sector and optionally verify RDB block type + checksum
static ULONG GetBlock(ULONG lba, ULONG type, UBYTE* buffer, struct DevBase* db)
{
    if (SdReadSector(db->db_CardType, lba, buffer) != 0)
    {
        kprintf("GetBlock: read error LBA %ld\n", lba);
        return 0;
    }

    if (type)
    {
        ULONG* p = (ULONG*)buffer;
        if (p[0] != type)
            return 0;

        // Checksum verification: sum of first p[1] longwords must be 0
        ULONG sum = 0;
        ULONG count = p[1];
        for (ULONG i = 0; i < count; i++)
            sum += p[i];

        if (sum != 0)
        {
            kprintf("GetBlock: checksum fail LBA %ld\n", lba);
            return 0;
        }
    }

    return 1; // success
}

// Context for LSEG block chain reader
struct ReadLSEGContext
{
    struct LoadSegBlock* lsb;
    struct DevBase* db;
    void* buffer;
    ULONG bytesLeft;
};

static ULONG ReadLSEG(void* readHandle, void* buffer, ULONG length)
{
    struct ReadLSEGContext* ctx = (struct ReadLSEGContext*)readHandle;
    struct LoadSegBlock* lsb = ctx->lsb;
    ULONG bytesRead = 0;

    while (length)
    {
        if (!ctx->bytesLeft)
        {
            ULONG lba = lsb->lsb_Next;
            if (lba == 0xFFFFFFFF)
                break;
            if (!GetBlock(lba, IDNAME_LOADSEG, (UBYTE*)lsb, ctx->db))
                break;
            ctx->buffer = &lsb->lsb_LoadData[0];
            ctx->bytesLeft = lsb->lsb_SummedLongs * sizeof(ULONG)
                           - __builtin_offsetof(struct LoadSegBlock, lsb_LoadData);
        }

        ULONG copyLen = (length <= ctx->bytesLeft) ? length : ctx->bytesLeft;

        struct ExecBase* SysBase = ctx->db->db_SysBase;
        CopyMem(ctx->buffer, buffer, copyLen);

        length -= copyLen;
        ctx->bytesLeft -= copyLen;
        bytesRead += copyLen;
        ctx->buffer = (UBYTE*)ctx->buffer + copyLen;
        buffer = (UBYTE*)buffer + copyLen;
    }

    return bytesRead;
}

static void* ExecAllocVec(ULONG size, ULONG flags)
{
    struct ExecBase* SysBase = *(struct ExecBase**)4;
    return AllocVec(size, flags);
}

static void ExecFreeVec(void* memory)
{
    struct ExecBase* SysBase = *(struct ExecBase**)4;
    FreeVec(memory);
}

static BPTR LoadFileSysFromLSEG(ULONG lba, struct DevBase* db)
{
    struct ExecBase* SysBase = db->db_SysBase;

    struct LoadSegBlock* lsb = AllocMem(BLOCK_SIZE, MEMF_PUBLIC | MEMF_CLEAR);
    if (!lsb)
        return 0;

    lsb->lsb_Next = lba;

    struct ReadLSEGContext ctx =
    {
        .lsb = lsb,
        .db = db,
        .buffer = NULL,
        .bytesLeft = 0
    };

    struct LoadSegFuncs funcs;
    funcs.read = ReadLSEG;
    funcs.alloc = ExecAllocVec;
    funcs.free = ExecFreeVec;

    BPTR segList = CustomLoadSeg(&ctx, 0, &funcs);

    FreeMem(lsb, BLOCK_SIZE);

    // Flush caches — loaded filesystem code needs I-cache invalidation on 68020+
    if (segList)
        CacheClearU();

    return segList;
}

void MountUnit(struct DevBase* db)
{
    struct ExecBase* SysBase = db->db_SysBase;

    kprintf("MountUnit: scanning for RDB\n");

    UBYTE* blockBuf = AllocMem(BLOCK_SIZE, MEMF_PUBLIC | MEMF_CLEAR);
    if (!blockBuf)
    {
        kprintf("MountUnit: alloc failed\n");
        return;
    }

    // Scan sectors 0-15 for RDB
    struct RigidDiskBlock* rdb = NULL;
    for (ULONG lba = 0; lba < RDB_LOCATION_LIMIT; lba++)
    {
        if (GetBlock(lba, IDNAME_RIGIDDISK, blockBuf, db))
        {
            rdb = (struct RigidDiskBlock*)blockBuf;
            kprintf("MountUnit: RDB found at LBA %ld\n", lba);
            break;
        }
    }

    if (!rdb)
    {
        kprintf("MountUnit: no RDB found\n");
        FreeMem(blockBuf, BLOCK_SIZE);
        return;
    }

    kprintf("  Cylinders: %ld, Heads: %ld, Sectors: %ld\n",
        rdb->rdb_Cylinders, rdb->rdb_Heads, rdb->rdb_Sectors);

    // Store total sectors
    db->db_TotalSectors = rdb->rdb_Cylinders * rdb->rdb_Heads * rdb->rdb_Sectors;
    kprintf("  TotalSectors: %ld\n", db->db_TotalSectors);

    // Open expansion.library for MakeDosNode/AddBootNode
    struct ExpansionBase* ExpansionBase = (struct ExpansionBase*)OpenLibrary(
        (CONST_STRPTR)"expansion.library", 33
    );
    if (!ExpansionBase)
    {
        kprintf("MountUnit: can't open expansion.library\n");
        FreeMem(blockBuf, BLOCK_SIZE);
        return;
    }

    // Open FileSystem.resource
    struct FileSysResource* fsr = (struct FileSysResource*)OpenResource((CONST_STRPTR)FSRNAME);

    // Walk FileSysHeader list — load filesystem handlers from LSEG blocks
    UBYTE* fhbBuf = AllocMem(BLOCK_SIZE, MEMF_PUBLIC | MEMF_CLEAR);
    if (fhbBuf && rdb->rdb_FileSysHeaderList != 0xFFFFFFFF)
    {
        struct FileSysHeaderBlock* fhb = (struct FileSysHeaderBlock*)fhbBuf;
        for (ULONG lba = rdb->rdb_FileSysHeaderList; lba != 0xFFFFFFFF; lba = fhb->fhb_Next)
        {
            if (!GetBlock(lba, IDNAME_FILESYSHEADER, fhbBuf, db))
                break;

            kprintf("  FSHD: DosType=%lx Version=%lx\n", fhb->fhb_DosType, fhb->fhb_Version);

            // Check if this filesystem version is already in FileSystem.resource
            if (fsr)
            {
                UBYTE alreadyLoaded = 0;
                struct Node* node;
                for (node = fsr->fsr_FileSysEntries.lh_Head; node->ln_Succ; node = node->ln_Succ)
                {
                    struct FileSysEntry* fse = (struct FileSysEntry*)node;
                    if (fhb->fhb_DosType == fse->fse_DosType && fhb->fhb_Version <= fse->fse_Version)
                    {
                        alreadyLoaded = 1;
                        break;
                    }
                }
                if (alreadyLoaded)
                {
                    kprintf("  FSHD: already loaded (same or newer)\n");
                    continue;
                }
            }

            // Load LSEG chain
            if (fhb->fhb_SegListBlocks == 0xFFFFFFFF)
                continue;

            BPTR segList = LoadFileSysFromLSEG(fhb->fhb_SegListBlocks, db);
            if (!segList)
            {
                kprintf("  FSHD: failed to load LSEG\n");
                continue;
            }

            kprintf("  FSHD: loaded segList=%08lx\n", segList);

            // Create FileSysEntry and add to FileSystem.resource
            if (fsr)
            {
                struct FileSysEntry* fse = AllocMem(sizeof(struct FileSysEntry), MEMF_PUBLIC | MEMF_CLEAR);
                if (fse)
                {
                    CopyMem(&fhb->fhb_DosType, &fse->fse_DosType,
                        sizeof(struct FileSysEntry) - __builtin_offsetof(struct FileSysEntry, fse_DosType));
                    fse->fse_SegList = segList;
                    AddHead(&fsr->fsr_FileSysEntries, &fse->fse_Node);
                }
            }
        }
    }

    // Walk Partition list — create DOS nodes and add boot nodes
    if (rdb->rdb_PartitionList != 0xFFFFFFFF)
    {
        UBYTE* pbBuf = AllocMem(BLOCK_SIZE, MEMF_PUBLIC | MEMF_CLEAR);
        if (pbBuf)
        {
            struct PartitionBlock* pb = (struct PartitionBlock*)pbBuf;
            for (ULONG lba = rdb->rdb_PartitionList; lba != 0xFFFFFFFF; lba = pb->pb_Next)
            {
                if (!GetBlock(lba, IDNAME_PARTITION, pbBuf, db))
                    break;

                struct DosEnvec* de = (struct DosEnvec*)pb->pb_Environment;

                // Null-terminate the BCPL drive name for logging
                pb->pb_DriveName[pb->pb_DriveName[0] + 1] = 0;
                kprintf("  PART: '%s' DosType=%lx LoCyl=%ld HiCyl=%ld\n",
                    &pb->pb_DriveName[1], de->de_DosType,
                    de->de_LowCyl, de->de_HighCyl);

                if (pb->pb_Flags & PBFF_NOMOUNT)
                {
                    kprintf("  PART: NOMOUNT, skipping\n");
                    continue;
                }

                // Build MakeDosNode parameter block
                // MakeDosNode expects: name, device, unit, flags, environment...
                // The environment vector starts 4 longs before pb_Environment
                ULONG* params = ((ULONG*)&pb->pb_Environment[0]) - 4;
                params[0] = (ULONG)&pb->pb_DriveName[1]; // name (C string, skipping length byte)
                params[1] = (ULONG)DeviceName;             // device name
                params[2] = 0;                             // unit number
                params[3] = pb->pb_DevFlags;               // device flags

                struct DeviceNode* dn = MakeDosNode(params);
                if (!dn)
                {
                    kprintf("  PART: MakeDosNode failed\n");
                    continue;
                }

                // Patch filesystem handler from FileSystem.resource
                if (fsr)
                {
                    struct Node* node;
                    for (node = fsr->fsr_FileSysEntries.lh_Head; node->ln_Succ; node = node->ln_Succ)
                    {
                        struct FileSysEntry* fse = (struct FileSysEntry*)node;
                        if (de->de_DosType != fse->fse_DosType)
                            continue;

                        kprintf("  PART: patching from FSE (ver %lx)\n", fse->fse_Version);

                        ULONG* src = &fse->fse_Type;
                        ULONG* dst = &dn->dn_Type;
                        for (ULONG patch = fse->fse_PatchFlags; patch; patch >>= 1)
                        {
                            if (patch & 0x1)
                                *dst = *src;
                            src++;
                            dst++;
                        }
                        break;
                    }
                }

                BYTE bootPri = 0;

                if (pb->pb_Flags & PBFF_BOOTABLE)
                {
                    struct FileSysStartupMsg* fssm = (struct FileSysStartupMsg*)BADDR(dn->dn_Startup);
                    struct DosEnvec* env = (struct DosEnvec*)BADDR(fssm->fssm_Environ);
                    bootPri = env->de_BootPri;
                    // Boost priority so tfpcmcia partitions always boot
                    // before internal drives (IDE pri ~0, DF0 pri 5).
                    // Preserve relative ordering between our partitions.
                    LONG boosted = (LONG)bootPri + 64;
                    if (boosted > 127)
                        boosted = 127;
                    bootPri = (BYTE)boosted;
                    kprintf("  PART: bootable, pri=%ld\n", (LONG)bootPri);
                }

                // KS3.1 bug: AddBootNode only sets NT_BOOTNODE when
                // ConfigDev is non-NULL. But passing a ConfigDev causes
                // the strap code to take the DiagArea path instead of
                // the SegList path. Fix: call with NULL ConfigDev then
                // manually patch the BootNode's ln_Type in the MountList.
                AddBootNode(bootPri, ADNF_STARTPROC, dn, NULL);

                // Walk eb_MountList and fix ln_Type on our BootNode
                struct BootNode* bn;
                for (bn = (struct BootNode*)ExpansionBase->MountList.lh_Head;
                     bn->bn_Node.ln_Succ;
                     bn = (struct BootNode*)bn->bn_Node.ln_Succ)
                {
                    if (bn->bn_DeviceNode == (APTR)dn)
                    {
                        bn->bn_Node.ln_Type = NT_BOOTNODE;
                        break;
                    }
                }
                kprintf("  PART: mounted\n");
            }

            FreeMem(pbBuf, BLOCK_SIZE);
        }
    }

    if (fhbBuf)
        FreeMem(fhbBuf, BLOCK_SIZE);

    CloseLibrary((struct Library*)ExpansionBase);
    FreeMem(blockBuf, BLOCK_SIZE);

    kprintf("MountUnit: done\n");
}
