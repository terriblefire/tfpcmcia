#pragma once

#include <exec/types.h>
#include <devices/scsidisk.h>

// SCSI command opcodes
#define SCSI_TEST_UNIT_READY  0x00
#define SCSI_REQUEST_SENSE    0x03
#define SCSI_READ_6           0x08
#define SCSI_WRITE_6          0x0A
#define SCSI_INQUIRY          0x12
#define SCSI_MODE_SENSE_6     0x1A
#define SCSI_READ_CAPACITY_10 0x25
#define SCSI_READ_10          0x28
#define SCSI_WRITE_10         0x2A

// SCSI status codes
#define SCSI_STATUS_GOOD            0x00
#define SCSI_STATUS_CHECK_CONDITION 0x02

// Sense keys
#define SENSE_NO_SENSE        0x00
#define SENSE_NOT_READY       0x02
#define SENSE_MEDIUM_ERROR    0x03
#define SENSE_ILLEGAL_REQUEST 0x05

struct DevBase;

BYTE HandleSCSICmd(struct IOStdReq* ior, struct DevBase* db);
