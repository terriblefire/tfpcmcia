#include <proto/exec.h>
#include <exec/errors.h>
#include <devices/trackdisk.h>

#include "device.h"
#include "debug.h"

// NSD command mapping table
static const struct NSDEntry NSDTable[] =
{
    { NSCMD_TD_READ64,   TD_READ64   },
    { NSCMD_TD_WRITE64,  TD_WRITE64  },
    { NSCMD_TD_FORMAT64, TD_FORMAT64 },
    { 0, 0 }
};

// Supported commands list for NSCMD_DEVICEQUERY
static const UWORD NSDSupported[] =
{
    CMD_READ,
    CMD_WRITE,
    CMD_UPDATE,
    CMD_CLEAR,
    TD_MOTOR,
    TD_CHANGENUM,
    TD_CHANGESTATE,
    TD_PROTSTATUS,
    TD_GETGEOMETRY,
    TD_READ64,
    TD_WRITE64,
    TD_FORMAT64,
    HD_SCSICMD,
    NSCMD_DEVICEQUERY,
    NSCMD_TD_READ64,
    NSCMD_TD_WRITE64,
    NSCMD_TD_FORMAT64,
    0
};

static void SendToTask(struct IOStdReq* ior, struct DevBase* db)
{
    struct ExecBase* SysBase = db->db_SysBase;
    ior->io_Flags &= ~IOF_QUICK;
    ior->io_Error = 0;
    PutMsg(db->db_IOPort, &ior->io_Message);
}

static void QuickIO(struct IOStdReq* ior, struct DevBase* db)
{
    if (ior->io_Flags & IOF_QUICK)
        return;
    struct ExecBase* SysBase = db->db_SysBase;
    ReplyMsg(&ior->io_Message);
}

static void DeviceQuery(struct IOStdReq* ior, struct DevBase* db)
{
    if (ior->io_Length < sizeof(struct NSDeviceQueryResult))
    {
        ior->io_Error = IOERR_BADLENGTH;
        return QuickIO(ior, db);
    }

    struct NSDeviceQueryResult* result = (struct NSDeviceQueryResult*)ior->io_Data;
    if (!result)
    {
        ior->io_Error = IOERR_BADADDRESS;
        return QuickIO(ior, db);
    }

    result->devQueryFormat = 0;
    result->sizeAvailable = sizeof(struct NSDeviceQueryResult);
    result->deviceType = NSDEVTYPE_TRACKDISK;
    result->deviceSubType = 0;
    result->supportedCommands = NSDSupported;
    ior->io_Actual = sizeof(struct NSDeviceQueryResult);
    ior->io_Error = 0;
    return QuickIO(ior, db);
}

static void GetGeometry(struct IOStdReq* ior, struct DevBase* db)
{
    struct DriveGeometry* dg = (struct DriveGeometry*)ior->io_Data;
    dg->dg_SectorSize = BLOCK_SIZE;
    dg->dg_TotalSectors = db->db_TotalSectors;
    dg->dg_Heads = GEOM_HEADS;
    dg->dg_TrackSectors = GEOM_SECTORS_TRACK;
    dg->dg_CylSectors = GEOM_HEADS * GEOM_SECTORS_TRACK;
    dg->dg_Cylinders = db->db_TotalSectors / (GEOM_HEADS * GEOM_SECTORS_TRACK);
    dg->dg_BufMemType = MEMF_PUBLIC;
    dg->dg_DeviceType = DG_DIRECT_ACCESS;
    dg->dg_Flags = 0;
    ior->io_Actual = sizeof(struct DriveGeometry);
    ior->io_Error = 0;
    return QuickIO(ior, db);
}

ULONG DevOpen(
    register ULONG unit __asm("d0"),
    register struct IOStdReq* ior __asm("a1"),
    register ULONG flags __asm("d1"),
    register struct DevBase* db __asm("a6")
)
{
    kprintf("DevOpen(unit:%ld, flags:%lx)\n", unit, flags);

    if (unit != 0 || db->db_CardType == SD_TYPE_NONE)
    {
        ior->io_Error = IOERR_OPENFAIL;
        ior->io_Device = NULL;
        return IOERR_OPENFAIL;
    }

    db->db_Device.dd_Library.lib_OpenCnt++;
    ior->io_Device = (struct Device*)db;
    ior->io_Unit = (struct Unit*)1; // single-unit device, non-NULL marker
    ior->io_Error = 0;
    return 0;
}

BPTR DevClose(
    register struct IOStdReq* ior __asm("a1"),
    register struct DevBase* db __asm("a6")
)
{
    db->db_Device.dd_Library.lib_OpenCnt--;
    ior->io_Device = NULL;
    ior->io_Unit = NULL;
    return 0;
}

BPTR DevExpunge(register struct DevBase* db __asm("a6"))
{
    return 0;
}

ULONG DevNull(void)
{
    return 0;
}

void DevBeginIO(
    register struct IOStdReq* ior __asm("a1"),
    register struct DevBase* db __asm("a6")
)
{
    UWORD cmd = ior->io_Command;

    // Handle NSD commands
    if (cmd > MAX_CMD)
    {
        if (cmd == NSCMD_DEVICEQUERY)
        {
            DeviceQuery(ior, db);
            return;
        }

        // Try NSD mapping
        for (const struct NSDEntry* e = NSDTable; e->nsCmd; e++)
        {
            if (e->nsCmd == cmd)
            {
                cmd = e->cmd;
                ior->io_Command = cmd;
                break;
            }
        }

        // Still out of range after mapping?
        if (ior->io_Command > MAX_CMD)
        {
            ior->io_Error = IOERR_NOCMD;
            ior->io_Actual = 0;
            QuickIO(ior, db);
            return;
        }
    }

    switch (cmd)
    {
        case CMD_READ:
        case CMD_WRITE:
        case TD_READ64:
        case TD_WRITE64:
        case TD_FORMAT64:
        case HD_SCSICMD:
        case CMD_RESET:
            SendToTask(ior, db);
            break;

        case CMD_UPDATE:
        case CMD_CLEAR:
        case TD_MOTOR:
            ior->io_Error = 0;
            ior->io_Actual = 0;
            QuickIO(ior, db);
            break;

        case TD_CHANGENUM:
        case TD_CHANGESTATE:
        case TD_PROTSTATUS:
            ior->io_Error = 0;
            ior->io_Actual = 0;
            QuickIO(ior, db);
            break;

        case TD_GETGEOMETRY:
            GetGeometry(ior, db);
            break;

        default:
            ior->io_Error = IOERR_NOCMD;
            ior->io_Actual = 0;
            QuickIO(ior, db);
            break;
    }
}

ULONG DevAbortIO(
    register struct IOStdReq* ior __asm("a1"),
    register struct DevBase* db __asm("a6")
)
{
    return IOERR_NOCMD;
}
