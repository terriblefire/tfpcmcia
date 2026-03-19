#include <proto/exec.h>
#include <exec/errors.h>
#include <devices/scsidisk.h>

#include "device.h"
#include "scsi.h"
#include "spi.h"
#include "debug.h"

static void SetSense(struct SCSICmd* scsi, UBYTE key, UBYTE asc, UBYTE ascq)
{
    if (!(scsi->scsi_Flags & SCSIF_AUTOSENSE) || !scsi->scsi_SenseData || scsi->scsi_SenseLength < 18)
        return;

    UBYTE* sense = scsi->scsi_SenseData;
    for (UWORD i = 0; i < scsi->scsi_SenseLength; i++)
        sense[i] = 0;

    sense[0] = 0x70;     // current error, fixed format
    sense[2] = key;       // sense key
    sense[7] = 10;        // additional sense length
    sense[12] = asc;      // ASC
    sense[13] = ascq;     // ASCQ
    scsi->scsi_SenseActual = 18;
}

static BYTE ScsiInquiry(struct SCSICmd* scsi, struct DevBase* db)
{
    UBYTE* data = (UBYTE*)scsi->scsi_Data;
    ULONG len = scsi->scsi_Length;
    if (len < 36)
        len = 36;

    for (UWORD i = 0; i < len; i++)
        data[i] = 0;

    data[0] = 0x00; // direct access device
    data[1] = 0x00; // not removable
    data[2] = 0x02; // SCSI-2
    data[3] = 0x02; // response data format
    data[4] = 31;   // additional length

    // Vendor (bytes 8-15)
    data[8]  = 'T';
    data[9]  = 'F';
    data[10] = ' ';
    data[11] = ' ';
    data[12] = ' ';
    data[13] = ' ';
    data[14] = ' ';
    data[15] = ' ';

    // Product (bytes 16-31)
    data[16] = 'P';
    data[17] = 'C';
    data[18] = 'M';
    data[19] = 'C';
    data[20] = 'I';
    data[21] = 'A';
    data[22] = ' ';
    data[23] = 'S';
    data[24] = 'D';
    data[25] = ' ';
    data[26] = ' ';
    data[27] = ' ';
    data[28] = ' ';
    data[29] = ' ';
    data[30] = ' ';
    data[31] = ' ';

    // Revision (bytes 32-35)
    data[32] = '1';
    data[33] = '.';
    data[34] = '0';
    data[35] = '0';

    scsi->scsi_Actual = len < scsi->scsi_Length ? len : scsi->scsi_Length;
    scsi->scsi_Status = SCSI_STATUS_GOOD;
    return 0;
}

static BYTE ScsiReadCapacity10(struct SCSICmd* scsi, struct DevBase* db)
{
    if (scsi->scsi_Length < 8)
    {
        scsi->scsi_Status = SCSI_STATUS_CHECK_CONDITION;
        SetSense(scsi, SENSE_ILLEGAL_REQUEST, 0x24, 0x00);
        return HFERR_BadStatus;
    }

    UBYTE* data = (UBYTE*)scsi->scsi_Data;
    ULONG lastLBA = db->db_TotalSectors ? db->db_TotalSectors - 1 : 0;

    // Last LBA (big-endian)
    data[0] = (lastLBA >> 24) & 0xFF;
    data[1] = (lastLBA >> 16) & 0xFF;
    data[2] = (lastLBA >> 8) & 0xFF;
    data[3] = lastLBA & 0xFF;

    // Block size (big-endian)
    data[4] = 0;
    data[5] = 0;
    data[6] = (BLOCK_SIZE >> 8) & 0xFF;
    data[7] = BLOCK_SIZE & 0xFF;

    scsi->scsi_Actual = 8;
    scsi->scsi_Status = SCSI_STATUS_GOOD;
    return 0;
}

static BYTE ScsiModeSense6(struct SCSICmd* scsi, struct DevBase* db)
{
    UBYTE* data = (UBYTE*)scsi->scsi_Data;
    ULONG len = scsi->scsi_Length;
    if (len < 4)
    {
        scsi->scsi_Status = SCSI_STATUS_CHECK_CONDITION;
        SetSense(scsi, SENSE_ILLEGAL_REQUEST, 0x24, 0x00);
        return HFERR_BadStatus;
    }

    for (UWORD i = 0; i < len; i++)
        data[i] = 0;

    data[0] = 3;    // mode data length (3 bytes follow)
    data[1] = 0;    // medium type
    data[2] = 0;    // device-specific: not write-protected
    data[3] = 0;    // block descriptor length

    scsi->scsi_Actual = 4;
    scsi->scsi_Status = SCSI_STATUS_GOOD;
    return 0;
}

static BYTE ScsiRead10(struct SCSICmd* scsi, struct DevBase* db)
{
    UBYTE* cdb = scsi->scsi_Command;

    ULONG lba = ((ULONG)cdb[2] << 24) | ((ULONG)cdb[3] << 16) |
                ((ULONG)cdb[4] << 8)  | (ULONG)cdb[5];
    UWORD count = ((UWORD)cdb[7] << 8) | (UWORD)cdb[8];

    UBYTE* data = (UBYTE*)scsi->scsi_Data;
    scsi->scsi_Actual = 0;

    for (UWORD i = 0; i < count; i++)
    {
        if (SdReadSector(db->db_CardType, lba + i, data) != 0)
        {
            kprintf("SCSI READ(10) error LBA %ld\n", lba + i);
            scsi->scsi_Status = SCSI_STATUS_CHECK_CONDITION;
            SetSense(scsi, SENSE_MEDIUM_ERROR, 0x11, 0x00);
            return HFERR_BadStatus;
        }
        data += BLOCK_SIZE;
        scsi->scsi_Actual += BLOCK_SIZE;
    }

    scsi->scsi_Status = SCSI_STATUS_GOOD;
    return 0;
}

static BYTE ScsiWrite10(struct SCSICmd* scsi, struct DevBase* db)
{
    UBYTE* cdb = scsi->scsi_Command;

    ULONG lba = ((ULONG)cdb[2] << 24) | ((ULONG)cdb[3] << 16) |
                ((ULONG)cdb[4] << 8)  | (ULONG)cdb[5];
    UWORD count = ((UWORD)cdb[7] << 8) | (UWORD)cdb[8];

    UBYTE* data = (UBYTE*)scsi->scsi_Data;
    scsi->scsi_Actual = 0;

    for (UWORD i = 0; i < count; i++)
    {
        if (SdWriteSector(db->db_CardType, lba + i, (const UBYTE*)data) != 0)
        {
            kprintf("SCSI WRITE(10) error LBA %ld\n", lba + i);
            scsi->scsi_Status = SCSI_STATUS_CHECK_CONDITION;
            SetSense(scsi, SENSE_MEDIUM_ERROR, 0x03, 0x00);
            return HFERR_BadStatus;
        }
        data += BLOCK_SIZE;
        scsi->scsi_Actual += BLOCK_SIZE;
    }

    scsi->scsi_Status = SCSI_STATUS_GOOD;
    return 0;
}

static BYTE ScsiRead6(struct SCSICmd* scsi, struct DevBase* db)
{
    UBYTE* cdb = scsi->scsi_Command;

    ULONG lba = ((ULONG)(cdb[1] & 0x1F) << 16) | ((ULONG)cdb[2] << 8) | (ULONG)cdb[3];
    UWORD count = cdb[4] ? cdb[4] : 256;

    UBYTE* data = (UBYTE*)scsi->scsi_Data;
    scsi->scsi_Actual = 0;

    for (UWORD i = 0; i < count; i++)
    {
        if (SdReadSector(db->db_CardType, lba + i, data) != 0)
        {
            scsi->scsi_Status = SCSI_STATUS_CHECK_CONDITION;
            SetSense(scsi, SENSE_MEDIUM_ERROR, 0x11, 0x00);
            return HFERR_BadStatus;
        }
        data += BLOCK_SIZE;
        scsi->scsi_Actual += BLOCK_SIZE;
    }

    scsi->scsi_Status = SCSI_STATUS_GOOD;
    return 0;
}

static BYTE ScsiWrite6(struct SCSICmd* scsi, struct DevBase* db)
{
    UBYTE* cdb = scsi->scsi_Command;

    ULONG lba = ((ULONG)(cdb[1] & 0x1F) << 16) | ((ULONG)cdb[2] << 8) | (ULONG)cdb[3];
    UWORD count = cdb[4] ? cdb[4] : 256;

    UBYTE* data = (UBYTE*)scsi->scsi_Data;
    scsi->scsi_Actual = 0;

    for (UWORD i = 0; i < count; i++)
    {
        if (SdWriteSector(db->db_CardType, lba + i, (const UBYTE*)data) != 0)
        {
            scsi->scsi_Status = SCSI_STATUS_CHECK_CONDITION;
            SetSense(scsi, SENSE_MEDIUM_ERROR, 0x03, 0x00);
            return HFERR_BadStatus;
        }
        data += BLOCK_SIZE;
        scsi->scsi_Actual += BLOCK_SIZE;
    }

    scsi->scsi_Status = SCSI_STATUS_GOOD;
    return 0;
}

BYTE HandleSCSICmd(struct IOStdReq* ior, struct DevBase* db)
{
    struct SCSICmd* scsi = (struct SCSICmd*)ior->io_Data;
    if (!scsi || !scsi->scsi_Command)
    {
        return IOERR_BADADDRESS;
    }

    UBYTE opcode = scsi->scsi_Command[0];
    scsi->scsi_SenseActual = 0;
    scsi->scsi_CmdActual = scsi->scsi_CmdLength;

    kprintf("SCSI cmd $%02lx\n", (ULONG)opcode);

    switch (opcode)
    {
        case SCSI_TEST_UNIT_READY:
            scsi->scsi_Status = SCSI_STATUS_GOOD;
            return 0;

        case SCSI_REQUEST_SENSE:
        {
            // Return all-zeros sense (no error pending)
            UBYTE* data = (UBYTE*)scsi->scsi_Data;
            ULONG len = scsi->scsi_Length;
            for (UWORD i = 0; i < len; i++)
                data[i] = 0;
            data[0] = 0x70; // current error
            data[7] = 10;   // additional sense length
            scsi->scsi_Actual = len;
            scsi->scsi_Status = SCSI_STATUS_GOOD;
            return 0;
        }

        case SCSI_INQUIRY:
            return ScsiInquiry(scsi, db);

        case SCSI_MODE_SENSE_6:
            return ScsiModeSense6(scsi, db);

        case SCSI_READ_CAPACITY_10:
            return ScsiReadCapacity10(scsi, db);

        case SCSI_READ_10:
            return ScsiRead10(scsi, db);

        case SCSI_WRITE_10:
            return ScsiWrite10(scsi, db);

        case SCSI_READ_6:
            return ScsiRead6(scsi, db);

        case SCSI_WRITE_6:
            return ScsiWrite6(scsi, db);

        default:
            kprintf("SCSI: unsupported cmd $%02lx\n", (ULONG)opcode);
            scsi->scsi_Status = SCSI_STATUS_CHECK_CONDITION;
            SetSense(scsi, SENSE_ILLEGAL_REQUEST, 0x20, 0x00); // invalid command
            return HFERR_BadStatus;
    }
}
