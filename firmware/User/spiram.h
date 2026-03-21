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

/* Pipelined 6-byte read: 4 cmd/addr bytes (RX discarded) + 2 data bytes.
 * After RXNE fires for byte N, TXE is already set (byte N+1 moved to SR),
 * so the next write is safe without an explicit TXE check. */
static __attribute__((always_inline)) uint16_t SPIRAM_Read16(uint32_t addr) {
    SPIRAM_CS_LOW();
    /* Prime: fill TX with first 2 bytes */
    while (!(SPI1->STATR & SPI_I2S_FLAG_TXE)) {}
    SPI1->DATAR = APS_CMD_READ;
    while (!(SPI1->STATR & SPI_I2S_FLAG_TXE)) {}
    SPI1->DATAR = (uint8_t)(addr >> 16);
    /* Pipeline: drain + send interleaved */
    while (!(SPI1->STATR & SPI_I2S_FLAG_RXNE)) {}
    (void)SPI1->DATAR; SPI1->DATAR = (uint8_t)(addr >> 8);
    while (!(SPI1->STATR & SPI_I2S_FLAG_RXNE)) {}
    (void)SPI1->DATAR; SPI1->DATAR = (uint8_t)(addr);
    while (!(SPI1->STATR & SPI_I2S_FLAG_RXNE)) {}
    (void)SPI1->DATAR; SPI1->DATAR = 0xFF;
    while (!(SPI1->STATR & SPI_I2S_FLAG_RXNE)) {}
    (void)SPI1->DATAR; SPI1->DATAR = 0xFF;
    /* Collect the 2 data bytes */
    while (!(SPI1->STATR & SPI_I2S_FLAG_RXNE)) {}
    uint8_t hi = (uint8_t)SPI1->DATAR;
    while (!(SPI1->STATR & SPI_I2S_FLAG_RXNE)) {}
    uint8_t lo = (uint8_t)SPI1->DATAR;
    SPIRAM_CS_HIGH();
    return ((uint16_t)hi << 8) | lo;
}

/* Pipelined 6-byte write: cmd + addr(3) + data(2), all RX discarded. */
static __attribute__((always_inline)) void SPIRAM_Write16(uint32_t addr, uint16_t data) {
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
    (void)SPI1->DATAR; SPI1->DATAR = (uint8_t)(data >> 8);
    while (!(SPI1->STATR & SPI_I2S_FLAG_RXNE)) {}
    (void)SPI1->DATAR; SPI1->DATAR = (uint8_t)(data);
    /* Drain remaining 2 */
    while (!(SPI1->STATR & SPI_I2S_FLAG_RXNE)) {}
    (void)SPI1->DATAR;
    while (!(SPI1->STATR & SPI_I2S_FLAG_RXNE)) {}
    (void)SPI1->DATAR;
    SPIRAM_CS_HIGH();
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
