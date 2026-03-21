#ifndef __SPIRAM_H
#define __SPIRAM_H

#include "debug.h"
#include "ch32v30x_spi.h"

/* APS6404L commands */
#define APS_CMD_READ    0x03u
#define APS_CMD_WRITE   0x02u

/* Direct-mapped read cache: 8 lines × 1024 bytes = 8 KB in CH32 SRAM.
 * Address decomposition (22-bit byte address):
 *   [9:0]   byte offset within line  (1024 B)
 *   [12:10] line index               (8 lines)
 *   [21:13] tag                      (9 bits)
 * Writes are write-through; the cache line is invalidated after each write. */
#define SPIRAM_CACHE_LINES      8u
#define SPIRAM_CACHE_LINE_BYTES 1024u
#define SPIRAM_CACHE_LINE_WORDS 512u    /* uint16_t entries */

typedef struct {
    uint32_t tag;
    uint8_t  valid;
    uint16_t data[SPIRAM_CACHE_LINE_WORDS];
} SpiramCacheLine;

static SpiramCacheLine spiram_cache[SPIRAM_CACHE_LINES];

/* All of the following must be always_inline: PCMCIA_Handler uses
 * WCH-Interrupt-fast which does not save/restore caller-saved registers. */

static __attribute__((always_inline)) void SPIRAM_CS_LOW(void)  { GPIOA->BCR  = GPIO_Pin_1; }
static __attribute__((always_inline)) void SPIRAM_CS_HIGH(void) { GPIOA->BSHR = GPIO_Pin_1; }

static __attribute__((always_inline)) uint8_t spiram_xfer(uint8_t data) {
    while (SPI_I2S_GetFlagStatus(SPI1, SPI_I2S_FLAG_TXE) == RESET) {}
    SPI_I2S_SendData(SPI1, data);
    while (SPI_I2S_GetFlagStatus(SPI1, SPI_I2S_FLAG_RXNE) == RESET) {}
    return (uint8_t)SPI_I2S_ReceiveData(SPI1);
}

/* Burst-fill dst[0..511] from SPIRAM starting at line_base (1024-byte aligned).
 * Runs entirely in 16-bit SPI mode; caller must hold CS low before entry —
 * this function asserts CS itself so callers need not manage it. */
static __attribute__((always_inline)) void SPIRAM_ReadBurst(uint32_t line_base, uint16_t *dst) {
    SPIRAM_CS_LOW();
    /* Switch to 16-bit words */
    SPI1->CTLR1 &= ~(1 << 6);              /* SPE = 0 */
    SPI1->CTLR1 |=  (1 << 11) | (1 << 6); /* DFF = 1, SPE = 1 */
    /* Send cmd+addr as 2 × 16-bit words */
    while (!(SPI1->STATR & SPI_I2S_FLAG_TXE)) {}
    SPI1->DATAR = ((uint16_t)APS_CMD_READ << 8) | (uint8_t)(line_base >> 16);
    while (!(SPI1->STATR & SPI_I2S_FLAG_TXE)) {}
    SPI1->DATAR = (uint16_t)(line_base & 0xFFFF);
    /* Drain the 2 address words, keep TX primed */
    while (!(SPI1->STATR & SPI_I2S_FLAG_RXNE)) {}
    (void)SPI1->DATAR; SPI1->DATAR = 0xFFFF;
    while (!(SPI1->STATR & SPI_I2S_FLAG_RXNE)) {}
    (void)SPI1->DATAR;
    /* Tight pipelined loop: prime next TX while reading current RX */
    for (uint32_t i = 0; i < SPIRAM_CACHE_LINE_WORDS - 1u; i++) {
        SPI1->DATAR = 0xFFFF;
        while (!(SPI1->STATR & SPI_I2S_FLAG_RXNE)) {}
        dst[i] = (uint16_t)SPI1->DATAR;
    }
    /* Final word: no TX write needed */
    while (!(SPI1->STATR & SPI_I2S_FLAG_RXNE)) {}
    dst[SPIRAM_CACHE_LINE_WORDS - 1u] = (uint16_t)SPI1->DATAR;
    SPIRAM_CS_HIGH();
    /* Restore 8-bit words */
    SPI1->CTLR1 &= ~(1 << 6);              /* SPE = 0 */
    SPI1->CTLR1 &= ~(1 << 11);             /* DFF = 0 */
    SPI1->CTLR1 |=  (1 << 6);              /* SPE = 1 */
}

/* Invalidate cache line if it holds addr. Called after every write. */
static __attribute__((always_inline)) void SPIRAM_CacheInvalidate(uint32_t addr) {
    uint32_t idx = (addr >> 10) & 7u;
    if (spiram_cache[idx].valid && spiram_cache[idx].tag == (addr >> 13))
        spiram_cache[idx].valid = 0;
}

/* Read 16-bit word. Returns cached data on hit; fills a 1 KB cache line on miss. */
static __attribute__((always_inline)) uint16_t SPIRAM_Read16(uint32_t addr) {
    uint32_t idx      = (addr >> 10) & 7u;
    uint32_t tag      = addr >> 13;
    uint32_t word_off = (addr >> 1) & (SPIRAM_CACHE_LINE_WORDS - 1u);
    SpiramCacheLine *line = &spiram_cache[idx];
    if (!line->valid || line->tag != tag) {
        /* Cache miss: burst-fill the 1 KB line */
        uint32_t line_base = addr & ~(SPIRAM_CACHE_LINE_BYTES - 1u);
        SPIRAM_ReadBurst(line_base, line->data);
        line->tag   = tag;
        line->valid = 1;
    }
    return line->data[word_off];
}

/* Pipelined 3-word write using 16-bit SPI words.
 *   Word 0 TX: [CMD | A23:16]
 *   Word 1 TX: [A15:8 | A7:0]
 *   Word 2 TX: [D15:8 | D7:0] */
static __attribute__((always_inline)) void SPIRAM_Write16(uint32_t addr, uint16_t data) {
    SPIRAM_CS_LOW();
    /* Switch to 16-bit words */
    SPI1->CTLR1 &= ~(1 << 6);              /* SPE = 0 */
    SPI1->CTLR1 |=  (1 << 11) | (1 << 6); /* DFF = 1, SPE = 1 */
    /* Prime with first 2 words */
    while (!(SPI1->STATR & SPI_I2S_FLAG_TXE)) {}
    SPI1->DATAR = ((uint16_t)APS_CMD_WRITE << 8) | (uint8_t)(addr >> 16);
    while (!(SPI1->STATR & SPI_I2S_FLAG_TXE)) {}
    SPI1->DATAR = (uint16_t)(addr & 0xFFFF);
    /* Pipeline: drain word 0, send data */
    while (!(SPI1->STATR & SPI_I2S_FLAG_RXNE)) {}
    (void)SPI1->DATAR; SPI1->DATAR = data;
    /* Drain words 1 and 2 */
    while (!(SPI1->STATR & SPI_I2S_FLAG_RXNE)) {}
    (void)SPI1->DATAR;
    while (!(SPI1->STATR & SPI_I2S_FLAG_RXNE)) {}
    (void)SPI1->DATAR;
    SPIRAM_CS_HIGH();
    /* Restore 8-bit words */
    SPI1->CTLR1 &= ~(1 << 6);              /* SPE = 0 */
    SPI1->CTLR1 &= ~(1 << 11);             /* DFF = 0 */
    SPI1->CTLR1 |=  (1 << 6);              /* SPE = 1 */
    SPIRAM_CacheInvalidate(addr);
}

/* Pipelined 5-byte write: cmd + addr(3) + data(1), all RX discarded. */
static __attribute__((always_inline)) void SPIRAM_Write8(uint32_t addr, uint8_t data) {
    SPIRAM_CS_LOW();
    /* Prime */
    while (!(SPI1->STATR & SPI_I2S_FLAG_TXE)) {}
    SPI1->DATAR = APS_CMD_WRITE;
    while (!(SPI1->STATR & SPI_I2S_FLAG_TXE)) {}
    SPI1->DATAR = (uint8_t)(addr >> 16);
    /* Pipeline */
    while (!(SPI1->STATR & SPI_I2S_FLAG_RXNE)) {}
    (void)SPI1->DATAR; SPI1->DATAR = (uint8_t)(addr >> 8);
    while (!(SPI1->STATR & SPI_I2S_FLAG_RXNE)) {}
    (void)SPI1->DATAR; SPI1->DATAR = (uint8_t)(addr);
    while (!(SPI1->STATR & SPI_I2S_FLAG_RXNE)) {}
    (void)SPI1->DATAR; SPI1->DATAR = data;
    /* Drain */
    while (!(SPI1->STATR & SPI_I2S_FLAG_RXNE)) {}
    (void)SPI1->DATAR;
    while (!(SPI1->STATR & SPI_I2S_FLAG_RXNE)) {}
    (void)SPI1->DATAR;
    SPIRAM_CS_HIGH();
    SPIRAM_CacheInvalidate(addr);
}

void SPIRAM_Init(void);

#endif /* __SPIRAM_H */
