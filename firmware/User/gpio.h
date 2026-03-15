#ifndef __GPIO_H
#define __GPIO_H

#include "debug.h"

void GPIO_Config(void);

/* PCMCIA Signals - Port A */
#define RAM_SNSS_GPIO_Port       GPIOA
#define RAM_SNSS_Pin             GPIO_Pin_1
#define READY_GPIO_Port          GPIOA
#define READY_Pin                GPIO_Pin_2
#define AUD_MIXED_GPIO_Port      GPIOA
#define AUD_MIXED_Pin            GPIO_Pin_4
#define RAM_CLK_GPIO_Port        GPIOA
#define RAM_CLK_Pin              GPIO_Pin_5
#define RAM_MISO_GPIO_Port       GPIOA
#define RAM_MISO_Pin             GPIO_Pin_6
#define RAM_MOSI_GPIO_Port       GPIOA
#define RAM_MOSI_Pin             GPIO_Pin_7
#define USART_TX_GPIO_Port       GPIOA
#define USART_TX_Pin             GPIO_Pin_9
#define USART_RX_GPIO_Port       GPIOA
#define USART_RX_Pin             GPIO_Pin_10
#define USB1_DM_GPIO_Port        GPIOA
#define USB1_DM_Pin              GPIO_Pin_11
#define USB1_DP_GPIO_Port        GPIOA
#define USB1_DP_Pin              GPIO_Pin_12
#define SWDIO_GPIO_Port          GPIOA
#define SWDIO_Pin                GPIO_Pin_13
#define SWCLK_GPIO_Port          GPIOA
#define SWCLK_Pin                GPIO_Pin_14
#define SD_CD_GPIO_Port          GPIOA
#define SD_CD_Pin                GPIO_Pin_15

/* PCMCIA Signals - Port B */
#define A16_GPIO_Port            GPIOB
#define A16_Pin                  GPIO_Pin_0
#define A17_GPIO_Port            GPIOB
#define A17_Pin                  GPIO_Pin_1
#define A18_GPIO_Port            GPIOB
#define A18_Pin                  GPIO_Pin_2
#define A19_GPIO_Port            GPIOB
#define A19_Pin                  GPIO_Pin_3
#define A20_GPIO_Port            GPIOB
#define A20_Pin                  GPIO_Pin_4
#define A21_GPIO_Port            GPIOB
#define A21_Pin                  GPIO_Pin_5
#define CE1_GPIO_Port            GPIOB
#define CE1_Pin                  GPIO_Pin_6
#define OE_GPIO_Port             GPIOB
#define OE_Pin                   GPIO_Pin_7
#define WE_GPIO_Port             GPIOB
#define WE_Pin                   GPIO_Pin_8
#define CE2_GPIO_Port            GPIOB
#define CE2_Pin                  GPIO_Pin_9
#define REG_GPIO_Port            GPIOB
#define REG_Pin                  GPIO_Pin_10
#define IORD_GPIO_Port           GPIOB
#define IORD_Pin                 GPIO_Pin_11
#define IOWR_GPIO_Port           GPIOB
#define IOWR_Pin                 GPIO_Pin_12
#define RESET_GPIO_Port          GPIOB
#define RESET_Pin                GPIO_Pin_13
#define WAIT_GPIO_Port           GPIOB
#define WAIT_Pin                 GPIO_Pin_14
#define IOCS16_GPIO_Port         GPIOB
#define IOCS16_Pin               GPIO_Pin_15

/* PCMCIA Signals - Port C */
#define LED_CLK_GPIO_Port        GPIOC
#define LED_CLK_Pin              GPIO_Pin_0
#define LED_DO_GPIO_Port         GPIOC
#define LED_DO_Pin               GPIO_Pin_1
#define LED2_GPIO_Port           GPIOC
#define LED2_Pin                 GPIO_Pin_7
#define LED1_GPIO_Port           GPIOC
#define LED1_Pin                 GPIO_Pin_8
#define SD_SNSS_GPIO_Port        GPIOC
#define SD_SNSS_Pin              GPIO_Pin_9
#define SD_CLK_GPIO_Port         GPIOC
#define SD_CLK_Pin               GPIO_Pin_10
#define SD_MISO_GPIO_Port        GPIOC
#define SD_MISO_Pin              GPIO_Pin_11
#define SD_MOSI_GPIO_Port        GPIOC
#define SD_MOSI_Pin              GPIO_Pin_12

/* PCMCIA Signals - Port D (Data Bus) */
#define D0_GPIO_Port             GPIOD
#define D0_Pin                   GPIO_Pin_0
#define D1_GPIO_Port             GPIOD
#define D1_Pin                   GPIO_Pin_1
#define D2_GPIO_Port             GPIOD
#define D2_Pin                   GPIO_Pin_2
#define D3_GPIO_Port             GPIOD
#define D3_Pin                   GPIO_Pin_3
#define D4_GPIO_Port             GPIOD
#define D4_Pin                   GPIO_Pin_4
#define D5_GPIO_Port             GPIOD
#define D5_Pin                   GPIO_Pin_5
#define D6_GPIO_Port             GPIOD
#define D6_Pin                   GPIO_Pin_6
#define D7_GPIO_Port             GPIOD
#define D7_Pin                   GPIO_Pin_7
#define D8_GPIO_Port             GPIOD
#define D8_Pin                   GPIO_Pin_8
#define D9_GPIO_Port             GPIOD
#define D9_Pin                   GPIO_Pin_9
#define D10_GPIO_Port            GPIOD
#define D10_Pin                  GPIO_Pin_10
#define D11_GPIO_Port            GPIOD
#define D11_Pin                  GPIO_Pin_11
#define D12_GPIO_Port            GPIOD
#define D12_Pin                  GPIO_Pin_12
#define D13_GPIO_Port            GPIOD
#define D13_Pin                  GPIO_Pin_13
#define D14_GPIO_Port            GPIOD
#define D14_Pin                  GPIO_Pin_14
#define D15_GPIO_Port            GPIOD
#define D15_Pin                  GPIO_Pin_15

/* PCMCIA Signals - Port E (Address Bus) */
#define A0_GPIO_Port             GPIOE
#define A0_Pin                   GPIO_Pin_0
#define A1_GPIO_Port             GPIOE
#define A1_Pin                   GPIO_Pin_1
#define A2_GPIO_Port             GPIOE
#define A2_Pin                   GPIO_Pin_2
#define A3_GPIO_Port             GPIOE
#define A3_Pin                   GPIO_Pin_3
#define A4_GPIO_Port             GPIOE
#define A4_Pin                   GPIO_Pin_4
#define A5_GPIO_Port             GPIOE
#define A5_Pin                   GPIO_Pin_5
#define A6_GPIO_Port             GPIOE
#define A6_Pin                   GPIO_Pin_6
#define A7_GPIO_Port             GPIOE
#define A7_Pin                   GPIO_Pin_7
#define A8_GPIO_Port             GPIOE
#define A8_Pin                   GPIO_Pin_8
#define A9_GPIO_Port             GPIOE
#define A9_Pin                   GPIO_Pin_9
#define A10_GPIO_Port            GPIOE
#define A10_Pin                  GPIO_Pin_10
#define A11_GPIO_Port            GPIOE
#define A11_Pin                  GPIO_Pin_11
#define A12_GPIO_Port            GPIOE
#define A12_Pin                  GPIO_Pin_12
#define A13_GPIO_Port            GPIOE
#define A13_Pin                  GPIO_Pin_13
#define A14_GPIO_Port            GPIOE
#define A14_Pin                  GPIO_Pin_14
#define A15_GPIO_Port            GPIOE
#define A15_Pin                  GPIO_Pin_15

#endif /* __GPIO_H */
