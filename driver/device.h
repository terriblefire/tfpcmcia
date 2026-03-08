#pragma once

#include <exec/devices.h>
#include <exec/io.h>
#include <exec/errors.h>
#include <dos/dos.h>
#include <devices/trackdisk.h>
#include <devices/scsidisk.h>

// Device version info
#define DEVICE_VERSION  1
#define DEVICE_REVISION 0
#define DEVICE_PRIORITY 4

#define DEVSTACK 4096

// SD card type
#define SD_TYPE_NONE 0
#define SD_TYPE_V1   1
#define SD_TYPE_V2   2
#define SD_TYPE_SDHC 3

// Block size
#define BLOCK_SIZE  512
#define BLOCK_SHIFT 9

// Geometry for fake CHS
#define GEOM_HEADS          16
#define GEOM_SECTORS_TRACK  32

// Commands
#ifndef TD_READ64
#define TD_READ64   24
#define TD_WRITE64  25
#define TD_SEEK64   26
#define TD_FORMAT64 27
#endif

#ifndef NSCMD_TD_READ64
#define NSCMD_TD_READ64     0xc000
#define NSCMD_TD_WRITE64    0xc001
#define NSCMD_TD_SEEK64     0xc002
#define NSCMD_TD_FORMAT64   0xc003
#define NSDEVTYPE_TRACKDISK 5
#endif

#ifndef NSCMD_DEVICEQUERY
#define NSCMD_DEVICEQUERY   0x4000
#endif

#define MAX_CMD HD_SCSICMD

// NSD mapping entry
struct NSDEntry
{
    UWORD nsCmd;
    UWORD cmd;
};

// NSD device query result
struct NSDeviceQueryResult
{
    ULONG  devQueryFormat;
    ULONG  sizeAvailable;
    UWORD  deviceType;
    UWORD  deviceSubType;
    const UWORD* supportedCommands;
};

// Device base structure
struct DevBase
{
    struct Device  db_Device;
    struct ExecBase* db_SysBase;
    BPTR           db_SegList;
    struct MsgPort* db_IOPort;
    struct Task*   db_IOTask;
    ULONG          db_TotalSectors;
    UBYTE          db_CardType;
    UBYTE          db_Pad[3];
};

// To use exec calls, declare: struct ExecBase* SysBase = db->db_SysBase;

// Device name string (defined in deviceheader.c)
extern const char DeviceName[];
extern const char DeviceIDString[];

// Function prototypes for device entry points (register params)
ULONG DevOpen(
    register ULONG unit __asm("d0"),
    register struct IOStdReq* ior __asm("a1"),
    register ULONG flags __asm("d1"),
    register struct DevBase* db __asm("a6")
);

BPTR DevClose(
    register struct IOStdReq* ior __asm("a1"),
    register struct DevBase* db __asm("a6")
);

BPTR DevExpunge(
    register struct DevBase* db __asm("a6")
);

ULONG DevNull(void);

void DevBeginIO(
    register struct IOStdReq* ior __asm("a1"),
    register struct DevBase* db __asm("a6")
);

ULONG DevAbortIO(
    register struct IOStdReq* ior __asm("a1"),
    register struct DevBase* db __asm("a6")
);

// IOTask
void IOTask(void);

// Mount
void MountUnit(struct DevBase* db);
