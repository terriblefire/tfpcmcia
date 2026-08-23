#ifndef __GPIO_H
#define __GPIO_H

#include "debug.h"

/* CH32V467VET6 pin map — docs/ch32v467-migration-plan.md §7.
 * Every Amiga-facing signal sits on a 5V-tolerant (FT) pad.
 * PE7/PE13/PE14/PE15 are not bonded out on this package. */

void GPIO_Config(void);

/* Control outputs - Port A */
#define WAIT_GPIO_Port           GPIOA
#define WAIT_Pin                 GPIO_Pin_8   /* active LOW: LOW inserts waits */
#define READY_GPIO_Port          GPIOA
#define READY_Pin                GPIO_Pin_9   /* active HIGH */
#define IOCS16_GPIO_Port         GPIOA
#define IOCS16_Pin               GPIO_Pin_10  /* LOW = 16-bit port */
#define RESET_GPIO_Port          GPIOA
#define RESET_Pin                GPIO_Pin_15  /* input, EXTI15 */

/* SD card - SPI1 default mapping (freed by dropping the external SPI RAM) */
#define SD_CLK_GPIO_Port         GPIOA
#define SD_CLK_Pin               GPIO_Pin_5
#define SD_MISO_GPIO_Port        GPIOA
#define SD_MISO_Pin              GPIO_Pin_6
#define SD_MOSI_GPIO_Port        GPIOA
#define SD_MOSI_Pin              GPIO_Pin_7
#define SD_SNSS_GPIO_Port        GPIOC
#define SD_SNSS_Pin              GPIO_Pin_4
#define SD_CD_GPIO_Port          GPIOC
#define SD_CD_Pin                GPIO_Pin_5   /* LOW = card inserted */

/* Debug - SWD on PA13/PA14 (default), USART1 remapped to PB6/PB7 */
#define SWDIO_GPIO_Port          GPIOA
#define SWDIO_Pin                GPIO_Pin_13
#define SWCLK_GPIO_Port          GPIOA
#define SWCLK_Pin                GPIO_Pin_14
#define USART_TX_GPIO_Port       GPIOB
#define USART_TX_Pin             GPIO_Pin_6
#define USART_RX_GPIO_Port       GPIOB
#define USART_RX_Pin             GPIO_Pin_7

/* Status LEDs + APA102 chain */
#define LED1_GPIO_Port           GPIOB
#define LED1_Pin                 GPIO_Pin_0
#define LED2_GPIO_Port           GPIOB
#define LED2_Pin                 GPIO_Pin_1
#define LED_CLK_GPIO_Port        GPIOC
#define LED_CLK_Pin              GPIO_Pin_0
#define LED_DO_GPIO_Port         GPIOC
#define LED_DO_Pin               GPIO_Pin_1

/* PB2 is BOOT1, strapped to GND on the board. Never drive it. */

/* PCMCIA control inputs - Port C (PC6..PC12, all FT).
 * Bit positions match the old PB6..PB12 layout, so every mask in
 * pcmcia.h keeps its value. */
#define CE1_GPIO_Port            GPIOC
#define CE1_Pin                  GPIO_Pin_6   /* EXTI6 */
#define OE_GPIO_Port             GPIOC
#define OE_Pin                   GPIO_Pin_7
#define WE_GPIO_Port             GPIOC
#define WE_Pin                   GPIO_Pin_8
#define CE2_GPIO_Port            GPIOC
#define CE2_Pin                  GPIO_Pin_9   /* EXTI9 */
#define REG_GPIO_Port            GPIOC
#define REG_Pin                  GPIO_Pin_10
#define IORD_GPIO_Port           GPIOC
#define IORD_Pin                 GPIO_Pin_11
#define IOWR_GPIO_Port           GPIOC
#define IOWR_Pin                 GPIO_Pin_12

/* Address bus (22 bits, split - PE7 does not exist on this package):
 *   A0-A6   PE0-PE6
 *   A7      PB3
 *   A8-A11  PE8-PE11
 *   A12-A19 PB8-PB15
 *   A20,A21 PB4,PB5
 * Reassembly macros live in pcmcia.h. */

/* Data bus D0-D15 on GPIOD - unchanged from the CH32V307 design */

#endif /* __GPIO_H */
