/*
 * spi.c — SD card SPI driver
 * Adapted from FPGAArcade Replay firmware card.c (MPL 2.0)
 * https://github.com/FPGAArcade/r1_firmware
 */

#include "spi.h"
#include "device.h"
#include "debug.h"

/* SysBase is unused here but SdSetSysBase() is kept for callers */
void SdSetSysBase(struct ExecBase* sb) { (void)sb; }

/* ---- Busy-wait ---------------------------------------------------------- */
/*
 * Each SpiByte call involves a PCMCIA bus cycle (250 ns min) plus the CH32
 * completing an SPI transfer at ~1.125 MHz (~7 µs/byte).  A conservative
 * lower bound is therefore ~7 µs per SpiByte call, independent of host CPU
 * speed (the CH32 firmware is the bottleneck, not the 68000).
 *
 * WaitMs() below is used only during card init (20 ms / 100 ms) where
 * precision doesn't matter — it just needs to be at least that long.
 * We emit 0xFF bytes with CS deasserted to fill the time; 1 ms ≈ 143 bytes.
 */
static void WaitMs(ULONG ms)
{
    ULONG bytes = ms * 150;   /* 150 SpiByte calls ≈ 1 ms */
    while (bytes--)
        SpiByte(0xFF);
}

/*
 * Countdown timeout: initialised to a number of polling iterations that
 * comfortably exceeds the required wall-clock window given ≥7 µs/byte.
 *
 *   250 ms token wait  → 50 000 iterations
 *   500 ms busy wait   → 100 000 iterations
 *  1000 ms init        → expressed as loop limit on ACMD41/CMD1 retries
 */
typedef ULONG Timeout;

static Timeout TimeoutNew(ULONG iters) { return iters; }
static BOOL    TimeoutDone(Timeout* t) { return *t == 0 ? TRUE : ((*t)--, FALSE); }

/* ---- SPI primitives ----------------------------------------------------- */

UBYTE SpiByte(UBYTE data)
{
    SPI_DATA = data;
    return SPI_DATA;
}

static UBYTE SpiCmd(UBYTE cmd, ULONG arg)
{
    UBYTE attempts = 100;
    while (SpiByte(0xFF) != 0xFF && attempts--)
        ;

    SpiByte(cmd);
    SpiByte((UBYTE)(arg >> 24));
    SpiByte((UBYTE)(arg >> 16));
    SpiByte((UBYTE)(arg >> 8));
    SpiByte((UBYTE)(arg));

    if      (cmd == CMD0) SpiByte(0x95);
    else if (cmd == CMD8) SpiByte(0x87);
    else                  SpiByte(0xFF);

    UBYTE r;
    for (UWORD i = 0; i < 256; i++) {
        r = SpiByte(0xFF);
        if (r != 0xFF) return r;
    }
    return 0xFF;
}

static void CsEnable(void)  { SPI_CS = 0x00; }
static void CsDisable(void) { SPI_CS = 0xFF; SpiByte(0xFF); }

/* ---- Single init attempt ------------------------------------------------ */

static UBYTE SdTryInit(void)
{
    UBYTE n, ocr[4], r1;

    /* 80+ dummy clocks with CS deasserted, then 20 ms settling */
    CsDisable();
    for (n = 0; n < 10; n++) SpiByte(0xFF);
    WaitMs(20);
    CsEnable();

    if (SpiCmd(CMD0, 0) != 0x01) {
        kprintf("SdInit: CMD0 fail\n");
        CsDisable();
        return SD_TYPE_NONE;
    }

    /* CMD8 — SEND_IF_COND (2.7-3.6 V) */
    r1 = SpiCmd(CMD8, 0x000001AA);
    for (n = 0; n < 4; n++) ocr[n] = SpiByte(0xFF);

    if (r1 == 0x01 && ocr[2] == 0x01 && ocr[3] == 0xAA) {
        /* SDv2 or SDHC — ACMD41 with HCS, up to ~10 000 attempts (~5 s) */
        kprintf("SdInit: SDv2\n");
        for (ULONG i = 0; i < 10000; i++) {
            CsDisable();
            CsEnable();
            if (SpiCmd(CMD55, 0) == 0x01 && SpiCmd(ACMD41, 0x40000000) == 0x00) {
                if (SpiCmd(CMD58, 0) == 0x00) {
                    for (n = 0; n < 4; n++) ocr[n] = SpiByte(0xFF);
                    UBYTE type = (ocr[0] & 0x40) ? SD_TYPE_SDHC : SD_TYPE_V2;
                    kprintf("SdInit: %s\n", type == SD_TYPE_SDHC ? "SDHC" : "SDv2");
                    if (type == SD_TYPE_V2) {
                        CsDisable(); CsEnable();
                        if (SpiCmd(CMD16, 512) != 0x00) kprintf("SdInit: CMD16 fail\n");
                    }
                    CsDisable();
                    return type;
                }
            }
        }
        kprintf("SdInit: SDv2 timeout\n");
        CsDisable();
        return SD_TYPE_NONE;
    }

    /* SDv1 or MMC */
    if (SpiCmd(CMD55, 0) <= 0x01) {
        kprintf("SdInit: SDv1\n");
        for (UWORD i = 0; i < 5000; i++) {
            CsDisable(); CsEnable();
            if (SpiCmd(CMD55, 0) == 0x01 && SpiCmd(ACMD41, 0) == 0x00) {
                if (SpiCmd(CMD16, 512) != 0x00) kprintf("SdInit: CMD16 fail\n");
                CsDisable();
                return SD_TYPE_V1;
            }
        }
        kprintf("SdInit: SDv1 timeout\n");
    } else {
        kprintf("SdInit: MMC\n");
        for (UWORD i = 0; i < 5000; i++) {
            if (SpiCmd(CMD1, 0) == 0x00) {
                if (SpiCmd(CMD16, 512) != 0x00) kprintf("SdInit: CMD16 fail\n");
                CsDisable();
                return SD_TYPE_V1;
            }
        }
        kprintf("SdInit: MMC timeout\n");
    }

    CsDisable();
    return SD_TYPE_NONE;
}

/* ---- Init with retries -------------------------------------------------- */

UBYTE SdInit(void)
{
    kprintf("SdInit: start\n");

    if (!(SPI_STATUS & 0x01)) {
        kprintf("SdInit: no card\n");
        return SD_TYPE_NONE;
    }

    kprintf("SdInit: card detected - try init\n");
    for (UBYTE attempt = 0; attempt < 3; attempt++) {
        kprintf("SdInit: attempt %ld\n", (ULONG)attempt);
        UBYTE type = SdTryInit();
        if (type != SD_TYPE_NONE) return type;
        WaitMs(100);
    }

    kprintf("SdInit: failed\n");
    return SD_TYPE_NONE;
}

/* ---- Read --------------------------------------------------------------- */

LONG SdReadSector(UBYTE cardType, ULONG lba, UBYTE* buffer)
{
    ULONG addr = (cardType == SD_TYPE_SDHC) ? lba : lba << 9;

    CsEnable();

    Timeout t = TimeoutNew(100000);
    while (SpiByte(0xFF) == 0x00)
        if (TimeoutDone(&t)) { kprintf("SdRead: busy lba %ld\n", lba); CsDisable(); return -1; }

    UBYTE r1 = SpiCmd(CMD17, addr);
    if (r1 != 0x00) {
        kprintf("SdRead: CMD17 fail %lx lba %ld\n", (ULONG)r1, lba);
        CsDisable();
        return -1;
    }

    t = TimeoutNew(50000);
    for (;;) {
        UBYTE tok = SpiByte(0xFF);
        if (tok == 0xFE) break;
        if (tok != 0xFF) { kprintf("SdRead: bad token %lx\n", (ULONG)tok); CsDisable(); return -1; }
        if (TimeoutDone(&t)) { kprintf("SdRead: token timeout lba %ld\n", lba); CsDisable(); return -1; }
    }

    for (UWORD i = 0; i < 512; i++) buffer[i] = SpiByte(0xFF);
    SpiByte(0xFF); SpiByte(0xFF);   /* CRC */

    r1 = SpiCmd(CMD13, 0);
    UBYTE r2 = SpiByte(0xFF);
    if (r1 != 0x00 || r2 != 0x00)
        kprintf("SdRead: CMD13 %lx.%lx lba %ld\n", (ULONG)r1, (ULONG)r2, lba);

    CsDisable();
    return 0;
}

/* ---- Write -------------------------------------------------------------- */

LONG SdWriteSector(UBYTE cardType, ULONG lba, const UBYTE* buffer)
{
    ULONG addr = (cardType == SD_TYPE_SDHC) ? lba : lba << 9;

    CsEnable();

    Timeout t = TimeoutNew(100000);
    while (SpiByte(0xFF) == 0x00)
        if (TimeoutDone(&t)) { kprintf("SdWrite: busy lba %ld\n", lba); CsDisable(); return -1; }

    UBYTE r1 = SpiCmd(CMD24, addr);
    if (r1 != 0x00) {
        kprintf("SdWrite: CMD24 fail %lx lba %ld\n", (ULONG)r1, lba);
        CsDisable();
        return -1;
    }

    SpiByte(0xFF);   /* gap */
    SpiByte(0xFE);   /* data token */
    for (UWORD i = 0; i < 512; i++) SpiByte(buffer[i]);
    SpiByte(0xFF); SpiByte(0xFF);   /* CRC */

    UBYTE resp = SpiByte(0xFF);
    if ((resp & 0x1F) != 0x05) {
        kprintf("SdWrite: bad resp %lx lba %ld\n", (ULONG)resp, lba);
        CsDisable();
        return -1;
    }

    t = TimeoutNew(100000);
    while (SpiByte(0xFF) == 0x00)
        if (TimeoutDone(&t)) { kprintf("SdWrite: prog timeout lba %ld\n", lba); CsDisable(); return -1; }

    r1 = SpiCmd(CMD13, 0);
    UBYTE r2 = SpiByte(0xFF);
    if (r1 != 0x00 || r2 != 0x00)
        kprintf("SdWrite: CMD13 %lx.%lx lba %ld\n", (ULONG)r1, (ULONG)r2, lba);

    CsDisable();
    return 0;
}
