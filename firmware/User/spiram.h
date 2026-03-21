#ifndef __SPIRAM_H
#define __SPIRAM_H

#include "debug.h"
#include "ch32v30x_spi.h"

/* APS6404L commands */
#define APS_CMD_READ    0x03u
#define APS_CMD_WRITE   0x02u

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

/* Pipelined 3-word read using 16-bit SPI words.
 * Wire layout (MSB-first per word):
 *   Word 0 TX: [CMD | A23:16]
 *   Word 1 TX: [A15:8 | A7:0]
 *   Word 2 TX: [0xFFFF]  →  RX: [D15:8 | D7:0]
 * DFF is toggled around the transaction; 8-bit mode is restored after. */
static __attribute__((always_inline)) uint16_t SPIRAM_Read16(uint32_t addr) {
    SPIRAM_CS_LOW();
    /* Switch to 16-bit words */
    SPI1->CTLR1 &= ~(1 << 6);              /* SPE = 0 */
    SPI1->CTLR1 |=  (1 << 11) | (1 << 6); /* DFF = 1, SPE = 1 */
    /* Prime with first 2 words */
    while (!(SPI1->STATR & SPI_I2S_FLAG_TXE)) {}
    SPI1->DATAR = ((uint16_t)APS_CMD_READ << 8) | (uint8_t)(addr >> 16);
    while (!(SPI1->STATR & SPI_I2S_FLAG_TXE)) {}
    SPI1->DATAR = (uint16_t)(addr & 0xFFFF);
    /* Pipeline: drain word 0, send dummy */
    while (!(SPI1->STATR & SPI_I2S_FLAG_RXNE)) {}
    (void)SPI1->DATAR; SPI1->DATAR = 0xFFFF;
    /* Drain word 1, collect data word */
    while (!(SPI1->STATR & SPI_I2S_FLAG_RXNE)) {}
    (void)SPI1->DATAR;
    while (!(SPI1->STATR & SPI_I2S_FLAG_RXNE)) {}
    uint16_t result = (uint16_t)SPI1->DATAR;
    SPIRAM_CS_HIGH();
    /* Restore 8-bit words */
    SPI1->CTLR1 &= ~(1 << 6);              /* SPE = 0 */
    SPI1->CTLR1 &= ~(1 << 11);             /* DFF = 0 */
    SPI1->CTLR1 |=  (1 << 6);              /* SPE = 1 */
    return result;
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
}

void SPIRAM_Init(void);

#endif /* __SPIRAM_H */
