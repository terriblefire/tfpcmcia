#include <proto/exec.h>
#include <exec/errors.h>
#include <devices/trackdisk.h>
#include <devices/scsidisk.h>

#include "device.h"
#include "spi.h"
#include "scsi.h"
#include "debug.h"


static void ProcessIOReq(struct IOStdReq* ior, struct DevBase* db);
static void ReadWrite(struct IOStdReq* ior, struct DevBase* db);

void IOTask(void)
{
    struct ExecBase* SysBase = *(struct ExecBase**)4;
    struct Task* thisTask = FindTask(NULL);
    struct DevBase* db = (struct DevBase*)thisTask->tc_UserData;

    kprintf("IOTask started\n");

    // The parent stored its task pointer in mp_SigTask temporarily
    struct MsgPort* port = db->db_IOPort;
    struct Task* parentTask = port->mp_SigTask;

    // Now set up the port for this task
    BYTE sigBit = AllocSignal(-1);
    port->mp_SigBit = sigBit;
    port->mp_SigTask = thisTask;

    ULONG sigMask = 1UL << sigBit;

    // Signal parent that we're ready
    Signal(parentTask, SIGF_SINGLE);

    // Main loop
    while (1)
    {
        Wait(sigMask);

        struct IOStdReq* ior;
        while ((ior = (struct IOStdReq*)GetMsg(port)) != NULL)
        {
            ProcessIOReq(ior, db);
        }
    }
}

static void ProcessIOReq(struct IOStdReq* ior, struct DevBase* db)
{
    struct ExecBase* SysBase = db->db_SysBase;

    ior->io_Error = 0;
    ior->io_Actual = 0;

    switch (ior->io_Command)
    {
        case CMD_READ:
        case CMD_WRITE:
        case TD_READ64:
        case TD_WRITE64:
        case TD_FORMAT64:
            ReadWrite(ior, db);
            break;

        case HD_SCSICMD:
            ior->io_Error = HandleSCSICmd(ior, db);
            break;

        case CMD_RESET:
            db->db_CardType = SdInit();
            ior->io_Error = (db->db_CardType == SD_TYPE_NONE) ? TDERR_DiskChanged : 0;
            break;

        default:
            ior->io_Error = IOERR_NOCMD;
            break;
    }

    ReplyMsg(&ior->io_Message);
}

static void ReadWrite(struct IOStdReq* ior, struct DevBase* db)
{
    UBYTE* buffer = (UBYTE*)ior->io_Data;
    ULONG length = ior->io_Length;
    UWORD cmd = ior->io_Command;

    if (length == 0)
        return;

    if (length & (BLOCK_SIZE - 1))
    {
        ior->io_Error = IOERR_BADLENGTH;
        return;
    }

    // Calculate LBA from offset
    ULONG offsetLow = ior->io_Offset;
    ULONG offsetHigh = 0;

    if (cmd == TD_READ64 || cmd == TD_WRITE64 || cmd == TD_FORMAT64)
    {
        offsetHigh = ior->io_Actual;
    }

    // LBA calculation using bit-shift trick (avoids 64-bit multiply)
    ULONG lba = (offsetHigh << (32 - BLOCK_SHIFT)) | (offsetLow >> BLOCK_SHIFT);

    ULONG sectors = length >> BLOCK_SHIFT;
    UBYTE isWrite = (cmd == CMD_WRITE || cmd == TD_WRITE64 || cmd == TD_FORMAT64);

    ior->io_Actual = 0;

    for (ULONG i = 0; i < sectors; i++)
    {
        LONG err;
        if (isWrite)
            err = SdWriteSector(db->db_CardType, lba + i, buffer);
        else
            err = SdReadSector(db->db_CardType, lba + i, buffer);

        if (err)
        {
            kprintf("IOTask: %s error at LBA %ld\n", isWrite ? "write" : "read", lba + i);
            ior->io_Error = TDERR_NotSpecified;
            return;
        }

        buffer += BLOCK_SIZE;
        ior->io_Actual += BLOCK_SIZE;
    }
}
