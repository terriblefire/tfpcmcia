#ifndef __PSRAM_H
#define __PSRAM_H

#include <stdint.h>

/* CH32V467 on-chip PSRAM, linearly memory-mapped at 0x8000_0000 (8 MB).
 * The Amiga window exposes the first 4 MB (A0-A21).
 *
 * This replaces the external APS6404L SPI RAM and its 32 KB write-back
 * cache (spiram.h): a cache miss cost a ~7 us SPI burst with the Amiga
 * held in /WAIT; a PSRAM access is a single load/store (tRC 60 ns).
 *
 * Byte-lane convention is inherited from the old cache: the even (UDS)
 * byte lives in the HIGH byte of the little-endian uint16_t, i.e. at
 * byte address (a | 1). PSRAM_Write8 therefore XOR-flips bit 0 so the
 * round trip through PSRAM_Read16/BUS16 is unchanged from the old code.
 */

#define PSRAM_MEM_BASE  0x80000000u
#define PSRAM_MEM_SIZE  0x00800000u  /* 8 MB on the CH32V467VET6 */

static __attribute__((always_inline)) inline uint16_t PSRAM_Read16(uint32_t addr) {
    return *(volatile uint16_t *)(PSRAM_MEM_BASE + (addr & ~1u));
}

static __attribute__((always_inline)) inline void PSRAM_Write16(uint32_t addr, uint16_t data) {
    *(volatile uint16_t *)(PSRAM_MEM_BASE + (addr & ~1u)) = data;
}

/* Read-modify-write on the containing 16-bit word rather than a direct
 * byte store: PSRAM byte writes are only supported on CH32V467 lots
 * whose fifth-last lot-number digit is not 0 (datasheet Table 3-42
 * note). The RMW form is safe on every lot at the cost of one extra
 * PSRAM cycle. On a confirmed byte-write-capable lot this can become
 *   *(volatile uint8_t *)(PSRAM_MEM_BASE + (addr ^ 1u)) = data;           */
static __attribute__((always_inline)) inline void PSRAM_Write8(uint32_t addr, uint8_t data) {
    volatile uint16_t *p = (volatile uint16_t *)(PSRAM_MEM_BASE + (addr & ~1u));
    uint16_t w = *p;
    if (addr & 1u)
        w = (w & 0xFF00u) | (uint16_t)data;         /* odd byte  -> low byte  */
    else
        w = (w & 0x00FFu) | ((uint16_t)data << 8);  /* even byte -> high byte */
    *p = w;
}

/* Controller + PSRAM-die init: clocks, timing, latency. Must run before
 * any access to the 0x8000_0000 window. */
void PSRAM_Init(void);

/* Walking-bit probe of the first word; returns 1 if PSRAM responds. */
int PSRAM_Probe(void);

#endif /* __PSRAM_H */
