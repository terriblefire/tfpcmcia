#ifndef __SPIRAM_H
#define __SPIRAM_H

#include "debug.h"
#include "ch32v30x_spi.h"

/* APS6404L commands */
#define APS_CMD_READ    0x03u
#define APS_CMD_WRITE   0x02u

/* Direct-mapped write-back cache: 512 lines × 64 bytes = 32 KB in CH32 SRAM.
 * Address decomposition (22-bit byte address):
 *   [5:0]   byte offset within line  (64 B)
 *   [14:6]  line index               (512 lines)
 *   [21:15] tag                      (7 bits)
 * Writes are cached; SPIRAM is written only when a dirty line is evicted. */
#define SPIRAM_CACHE_LINES      512u
#define SPIRAM_CACHE_LINE_BYTES 64u
#define SPIRAM_CACHE_LINE_WORDS 32u     /* uint16_t entries */

typedef struct {
    uint32_t tag;
    uint8_t  valid;
    uint8_t  dirty;
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

/* Burst-fill dst[0..255] from SPIRAM starting at line_base (512-byte aligned).
 * Runs entirely in 16-bit SPI mode. */
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

/* Burst-write src[0..255] to SPIRAM starting at line_base (512-byte aligned).
 * Runs entirely in 16-bit SPI mode. */
static __attribute__((always_inline)) void SPIRAM_WriteBurst(uint32_t line_base, uint16_t *src) {
    SPIRAM_CS_LOW();
    SPI1->CTLR1 &= ~(1 << 6);
    SPI1->CTLR1 |=  (1 << 11) | (1 << 6); /* 16-bit, SPE=1 */
    /* cmd+addr */
    while (!(SPI1->STATR & SPI_I2S_FLAG_TXE)) {}
    SPI1->DATAR = ((uint16_t)APS_CMD_WRITE << 8) | (uint8_t)(line_base >> 16);
    while (!(SPI1->STATR & SPI_I2S_FLAG_TXE)) {}
    SPI1->DATAR = (uint16_t)(line_base & 0xFFFF);
    /* Drain cmd word 0, prime data word 0 */
    while (!(SPI1->STATR & SPI_I2S_FLAG_RXNE)) {}
    (void)SPI1->DATAR; SPI1->DATAR = src[0];
    /* Pipelined data loop */
    for (uint32_t i = 1; i < SPIRAM_CACHE_LINE_WORDS; i++) {
        while (!(SPI1->STATR & SPI_I2S_FLAG_RXNE)) {}
        (void)SPI1->DATAR;
        SPI1->DATAR = src[i];
    }
    /* Drain final two words (response to src[N-2] and src[N-1]) */
    while (!(SPI1->STATR & SPI_I2S_FLAG_RXNE)) {}
    (void)SPI1->DATAR;
    while (!(SPI1->STATR & SPI_I2S_FLAG_RXNE)) {}
    (void)SPI1->DATAR;
    SPIRAM_CS_HIGH();
    SPI1->CTLR1 &= ~(1 << 6);
    SPI1->CTLR1 &= ~(1 << 11);
    SPI1->CTLR1 |=  (1 << 6);
}

/* Read 16-bit word. Returns cached data on hit; on miss, flushes dirty line
 * (if any) then fills a 512-byte cache line from SPIRAM. */
static __attribute__((always_inline)) uint16_t SPIRAM_Read16(uint32_t addr) {
    uint32_t idx      = (addr >> 6) & (SPIRAM_CACHE_LINES - 1u);
    uint32_t tag      = addr >> 15;
    uint32_t word_off = (addr >> 1) & (SPIRAM_CACHE_LINE_WORDS - 1u);
    SpiramCacheLine *line = &spiram_cache[idx];
    if (!line->valid || line->tag != tag) {
        if (line->valid && line->dirty)
            SPIRAM_WriteBurst((line->tag << 15) | (idx << 6), line->data);
        uint32_t line_base = addr & ~(SPIRAM_CACHE_LINE_BYTES - 1u);
        SPIRAM_ReadBurst(line_base, line->data);
        line->tag   = tag;
        line->valid = 1;
        line->dirty = 0;
    }
    return line->data[word_off];
}

/* Write 16-bit word. Caches the write; SPIRAM is not touched on a hit.
 * On miss, flushes dirty evicted line then fills the new line before writing. */
static __attribute__((always_inline)) void SPIRAM_Write16(uint32_t addr, uint16_t data) {
    uint32_t idx      = (addr >> 6) & (SPIRAM_CACHE_LINES - 1u);
    uint32_t tag      = addr >> 15;
    uint32_t word_off = (addr >> 1) & (SPIRAM_CACHE_LINE_WORDS - 1u);
    SpiramCacheLine *line = &spiram_cache[idx];
    if (!line->valid || line->tag != tag) {
        if (line->valid && line->dirty)
            SPIRAM_WriteBurst((line->tag << 15) | (idx << 6), line->data);
        uint32_t line_base = addr & ~(SPIRAM_CACHE_LINE_BYTES - 1u);
        SPIRAM_ReadBurst(line_base, line->data);
        line->tag   = tag;
        line->valid = 1;
        line->dirty = 0;
    }
    line->data[word_off] = data;
    line->dirty = 1;
}

/* Write single byte. Read-modify-write in cache; SPIRAM not touched on hit. */
static __attribute__((always_inline)) void SPIRAM_Write8(uint32_t addr, uint8_t data) {
    uint32_t word_addr = addr & ~1u;
    uint32_t idx       = (word_addr >> 6) & (SPIRAM_CACHE_LINES - 1u);
    uint32_t tag       = word_addr >> 15;
    uint32_t word_off  = (word_addr >> 1) & (SPIRAM_CACHE_LINE_WORDS - 1u);
    SpiramCacheLine *line = &spiram_cache[idx];
    if (!line->valid || line->tag != tag) {
        if (line->valid && line->dirty)
            SPIRAM_WriteBurst((line->tag << 15) | (idx << 6), line->data);
        uint32_t line_base = word_addr & ~(SPIRAM_CACHE_LINE_BYTES - 1u);
        SPIRAM_ReadBurst(line_base, line->data);
        line->tag   = tag;
        line->valid = 1;
        line->dirty = 0;
    }
    uint16_t w = line->data[word_off];
    if (addr & 1u)
        w = (w & 0xFF00u) | (uint16_t)data;         /* odd byte  → low byte  */
    else
        w = (w & 0x00FFu) | ((uint16_t)data << 8);  /* even byte → high byte */
    line->data[word_off] = w;
    line->dirty = 1;
}

void SPIRAM_Init(void);

#endif /* __SPIRAM_H */
