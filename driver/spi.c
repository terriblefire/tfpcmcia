#include "spi.h"
#include "device.h"
#include "debug.h"

// Transfer one byte over SPI
UBYTE SpiByte(UBYTE data)
{
    SPI_DATA = data;
    return SPI_DATA;
}

// Send an SD command and get R1 response
UBYTE SpiCmd(UBYTE cmd, ULONG arg)
{
    // flush - wait for bus idle
    UBYTE attempts = 100;
    while (SpiByte(0xFF) != 0xFF && attempts--)
        ;

    SpiByte(cmd);
    SpiByte((UBYTE)(arg >> 24));
    SpiByte((UBYTE)(arg >> 16));
    SpiByte((UBYTE)(arg >> 8));
    SpiByte((UBYTE)(arg));

    // CRC — only CMD0 and CMD8 need valid CRC
    if (cmd == CMD0)
        SpiByte(0x95);
    else if (cmd == CMD8)
        SpiByte(0x87);
    else
        SpiByte(0xFF);

    // poll for response (non-$FF byte)
    UBYTE response;
    for (UWORD i = 0; i < 256; i++)
    {
        response = SpiByte(0xFF);
        if (response != 0xFF)
            return response;
    }
    return 0xFF;
}

// Initialize SD card, return card type (SD_TYPE_xxx)
UBYTE SdInit(void)
{
    kprintf("SdInit: start\n");

    // check card detect
    if (!(SPI_STATUS & 0x01))
    {
        kprintf("SdInit: no card\n");
        return SD_TYPE_NONE;
    }

    // deassert CS, send 80+ clocks
    SPI_CS = 0xFF;
    for (int i = 0; i < 10; i++)
        SpiByte(0xFF);

    // assert CS
    SPI_CS = 0x00;

    // CMD0 — GO_IDLE_STATE
    UBYTE r1 = SpiCmd(CMD0, 0);
    if (r1 != 0x01)
    {
        kprintf("SdInit: CMD0 fail %lx\n", (ULONG)r1);
        SPI_CS = 0xFF;
        SpiByte(0xFF);
        return SD_TYPE_NONE;
    }

    // CMD8 — SEND_IF_COND
    r1 = SpiCmd(CMD8, 0x000001AA);
    // read 4 bytes of R7 response
    SpiByte(0xFF);
    SpiByte(0xFF);
    UBYTE cmd8r2 = SpiByte(0xFF);
    UBYTE cmd8r3 = SpiByte(0xFF);

    if (r1 == 0x01 && cmd8r2 == 0x01 && cmd8r3 == 0xAA)
    {
        // SDv2 card — ACMD41 with HCS
        kprintf("SdInit: SDv2\n");
        for (UWORD i = 0; i < 1000; i++)
        {
            SpiCmd(CMD55, 0);
            r1 = SpiCmd(ACMD41, 0x40000000);
            if (r1 == 0x00)
            {
                // check CCS bit via CMD58
                r1 = SpiCmd(CMD58, 0);
                UBYTE ocr0 = SpiByte(0xFF);
                SpiByte(0xFF);
                SpiByte(0xFF);
                SpiByte(0xFF);

                SPI_CS = 0xFF;
                SpiByte(0xFF);

                if (ocr0 & 0x40)
                {
                    kprintf("SdInit: SDHC\n");
                    return SD_TYPE_SDHC;
                }
                else
                {
                    kprintf("SdInit: SDv2 (byte)\n");
                    return SD_TYPE_V2;
                }
            }
        }

        kprintf("SdInit: ACMD41 timeout\n");
        SPI_CS = 0xFF;
        SpiByte(0xFF);
        return SD_TYPE_NONE;
    }

    // SDv1 — try ACMD41 without HCS
    kprintf("SdInit: SDv1\n");
    for (UWORD i = 0; i < 1000; i++)
    {
        SpiCmd(CMD55, 0);
        r1 = SpiCmd(ACMD41, 0);
        if (r1 == 0x00)
        {
            SPI_CS = 0xFF;
            SpiByte(0xFF);
            return SD_TYPE_V1;
        }
    }

    kprintf("SdInit: SDv1 ACMD41 timeout\n");
    SPI_CS = 0xFF;
    SpiByte(0xFF);
    return SD_TYPE_NONE;
}

// Read a single 512-byte sector
// lba is sector number; for non-SDHC cards this becomes byte address
LONG SdReadSector(ULONG lba, UBYTE* buffer)
{
    // For MAME emulated SD_TYPE_V2: byte address
    ULONG addr = lba;

    SPI_CS = 0x00;

    UBYTE r1 = SpiCmd(CMD17, addr);
    if (r1 != 0x00)
    {
        kprintf("SdRead: CMD17 fail %lx lba %ld\n", (ULONG)r1, lba);
        SPI_CS = 0xFF;
        SpiByte(0xFF);
        return -1;
    }

    // wait for data token $FE
    for (UWORD i = 0; i < 4096; i++)
    {
        UBYTE tok = SpiByte(0xFF);
        if (tok == 0xFE)
            goto gotToken;
        if (tok != 0xFF)
        {
            kprintf("SdRead: bad token %lx\n", (ULONG)tok);
            SPI_CS = 0xFF;
            SpiByte(0xFF);
            return -1;
        }
    }
    kprintf("SdRead: token timeout\n");
    SPI_CS = 0xFF;
    SpiByte(0xFF);
    return -1;

gotToken:
    // read 512 bytes
    for (UWORD i = 0; i < 512; i++)
        buffer[i] = SpiByte(0xFF);

    // discard CRC
    SpiByte(0xFF);
    SpiByte(0xFF);

    SPI_CS = 0xFF;
    SpiByte(0xFF);
    return 0;
}

// Write a single 512-byte sector
LONG SdWriteSector(ULONG lba, const UBYTE* buffer)
{
    ULONG addr = lba;

    SPI_CS = 0x00;

    UBYTE r1 = SpiCmd(CMD24, addr);
    if (r1 != 0x00)
    {
        kprintf("SdWrite: CMD24 fail %lx lba %ld\n", (ULONG)r1, lba);
        SPI_CS = 0xFF;
        SpiByte(0xFF);
        return -1;
    }

    // send data token
    SpiByte(0xFF);
    SpiByte(0xFE);

    // send 512 bytes
    for (UWORD i = 0; i < 512; i++)
        SpiByte(buffer[i]);

    // send dummy CRC
    SpiByte(0xFF);
    SpiByte(0xFF);

    // read data response
    UBYTE response = SpiByte(0xFF);
    if ((response & 0x1F) != 0x05)
    {
        kprintf("SdWrite: bad response %lx\n", (ULONG)response);
        SPI_CS = 0xFF;
        SpiByte(0xFF);
        return -1;
    }

    // wait for busy (card programming)
    for (UWORD i = 0; i < 65535; i++)
    {
        if (SpiByte(0xFF) != 0x00)
            break;
    }

    SPI_CS = 0xFF;
    SpiByte(0xFF);
    return 0;
}
