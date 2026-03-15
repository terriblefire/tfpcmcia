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

static __attribute__((always_inline)) uint16_t SPIRAM_Read16(uint32_t addr) {
    SPIRAM_CS_LOW();
    spiram_xfer(APS_CMD_READ);
    spiram_xfer((uint8_t)(addr >> 16));
    spiram_xfer((uint8_t)(addr >> 8));
    spiram_xfer((uint8_t)(addr));
    uint8_t hi = spiram_xfer(0xFF);
    uint8_t lo = spiram_xfer(0xFF);
    SPIRAM_CS_HIGH();
    return ((uint16_t)hi << 8) | lo;
}

static __attribute__((always_inline)) void SPIRAM_Write16(uint32_t addr, uint16_t data) {
    SPIRAM_CS_LOW();
    spiram_xfer(APS_CMD_WRITE);
    spiram_xfer((uint8_t)(addr >> 16));
    spiram_xfer((uint8_t)(addr >> 8));
    spiram_xfer((uint8_t)(addr));
    spiram_xfer((uint8_t)(data >> 8));
    spiram_xfer((uint8_t)(data));
    SPIRAM_CS_HIGH();
}

void SPIRAM_Init(void);
void SPIRAM_Test(void);

#endif /* __SPIRAM_H */
