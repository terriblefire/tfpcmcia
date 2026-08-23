#include "gpio.h"


static void GPIO_Spi_Init(SPI_TypeDef *spi, GPIO_TypeDef *gpio, uint16_t pins, uint16_t prescaler) {

    GPIO_InitTypeDef GPIO_InitStructure = {0};
    SPI_InitTypeDef SPI_InitStructure = {0};

    // SCK, MOSI pin configuration
    GPIO_InitStructure.GPIO_Pin = pins;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_High;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init (gpio, &GPIO_InitStructure);

    // SPI configuration
    SPI_InitStructure.SPI_Direction = SPI_Direction_2Lines_FullDuplex;
    SPI_InitStructure.SPI_Mode = SPI_Mode_Master;
    SPI_InitStructure.SPI_DataSize = SPI_DataSize_8b;
    SPI_InitStructure.SPI_CPOL = SPI_CPOL_Low;
    SPI_InitStructure.SPI_CPHA = SPI_CPHA_1Edge;
    SPI_InitStructure.SPI_NSS = SPI_NSS_Soft;
    SPI_InitStructure.SPI_BaudRatePrescaler = prescaler;
    SPI_InitStructure.SPI_FirstBit = SPI_FirstBit_MSB;
    SPI_InitStructure.SPI_CRCPolynomial = 7;
    SPI_Init(spi, &SPI_InitStructure);

    /* SSI=1: master sees NSS high (software NSS), prevents MODF fault */
    SPI_NSSInternalSoftwareConfig(spi, SPI_NSSInternalSoft_Set);

    SPI_Cmd(spi, ENABLE);
}

void GPIO_Config(void) {
    GPIO_InitTypeDef GPIO_InitStructure = {0};
    EXTI_InitTypeDef EXTI_InitStructure = {0};

    /* Enable clocks for all used GPIO ports and AFIO */
    RCC->PB2PCENR |= RCC_PB2Periph_GPIOA | RCC_PB2Periph_GPIOB |
                     RCC_PB2Periph_GPIOC | RCC_PB2Periph_GPIOD |
                     RCC_PB2Periph_GPIOE | RCC_PB2Periph_AFIO;

    /* Address low/mid A0-A6, A8-A11 on GPIOE: input floating.
     * PE7/PE13-PE15 are not bonded on the CH32V467VET6 (harmless to
     * configure); PE12 is BOOT0 with an external pull-down — floating
     * input is its reset state, leave it be. */
    GPIOE->CFGLR = 0x44444444;
    GPIOE->CFGHR = 0x44444444;

    /* Data bus D0-D15 on GPIOD: input floating (high-Z, tristate) */
    GPIOD->CFGLR = 0x44444444;
    GPIOD->CFGHR = 0x44444444;

    /* Address high A12-A19 on PB8-PB15: input floating */
    GPIOB->CFGHR = 0x44444444;

    /* PB3 (A7), PB4 (A20), PB5 (A21): input floating.
     * PB2 is BOOT1, strapped to GND on the board — leave it an input,
     * never drive it. */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2 | GPIO_Pin_3 | GPIO_Pin_4 | GPIO_Pin_5;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    /* PC6 (CE1), PC7 (OE), PC8 (WE), PC9 (CE2),
       PC10 (REG), PC11 (IOR), PC12 (IOW): input pull-up */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6 | GPIO_Pin_7 | GPIO_Pin_8 |
                                  GPIO_Pin_9 | GPIO_Pin_10 | GPIO_Pin_11 |
                                  GPIO_Pin_12;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_Init(GPIOC, &GPIO_InitStructure);

    /* PA8 (WAIT): output push-pull, drive HIGH (no wait states) */
    GPIO_InitStructure.GPIO_Pin = WAIT_Pin;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_High;
    GPIO_Init(WAIT_GPIO_Port, &GPIO_InitStructure);
    WAIT_GPIO_Port->BSHR = WAIT_Pin;      /* WAIT = HIGH */

    /* PA9 (READY): output push-pull, drive HIGH (card ready) */
    GPIO_InitStructure.GPIO_Pin = READY_Pin;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_High;
    GPIO_Init(READY_GPIO_Port, &GPIO_InitStructure);
    READY_GPIO_Port->BSHR = READY_Pin;    /* READY = HIGH */

    /* PA10 (IOIS16): output push-pull (LOW = 16-bit port) */
    GPIO_InitStructure.GPIO_Pin = IOCS16_Pin;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_High;
    GPIO_Init(IOCS16_GPIO_Port, &GPIO_InitStructure);
    IOCS16_GPIO_Port->BCR = IOCS16_Pin;   /* IOIS16 = LOW (16-bit) */

    /* PA15 (RESET from Gayle): input pull-up, EXTI15 below */
    GPIO_InitStructure.GPIO_Pin = RESET_Pin;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_Init(RESET_GPIO_Port, &GPIO_InitStructure);

    /* SD card on SPI1 default pins (PA5 SCK, PA6 MISO, PA7 MOSI).
     * PCLK2 = 200 MHz, prescaler 16 -> 12.5 MHz (SD spec allows 25). */
    RCC_PB2PeriphClockCmd(RCC_PB2Periph_SPI1, ENABLE);
    GPIO_Spi_Init(SPI1, GPIOA, SD_CLK_Pin | SD_MOSI_Pin, SPI_BaudRatePrescaler_16);
    /* MISO as input pull-up — holds MISO high when SD card releases the bus */
    GPIO_InitStructure.GPIO_Pin = SD_MISO_Pin;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_High;
    GPIO_Init(SD_MISO_GPIO_Port, &GPIO_InitStructure);

    /* PC4 (SD CS): output push-pull, drive HIGH (deasserted) */
    GPIO_InitStructure.GPIO_Pin = SD_SNSS_Pin;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_High;
    GPIO_Init(SD_SNSS_GPIO_Port, &GPIO_InitStructure);
    SD_SNSS_GPIO_Port->BSHR = SD_SNSS_Pin;

    /* PC5 (SD CD): input pull-up (LOW = card inserted) */
    GPIO_InitStructure.GPIO_Pin = SD_CD_Pin;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_Init(SD_CD_GPIO_Port, &GPIO_InitStructure);

    /* USART1 remapped to PB6 (TX) / PB7 (RX) — PA9/PA10 are READY and
     * IOIS16 on this board. */
    GPIO_PinRemapConfig(GPIO_PartialRemap1_USART1, ENABLE);  /* TX=PB6, RX=PB7 */
    GPIO_InitStructure.GPIO_Pin = USART_TX_Pin;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_High;
    GPIO_Init(USART_TX_GPIO_Port, &GPIO_InitStructure);
    GPIO_InitStructure.GPIO_Pin = USART_RX_Pin;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_Init(USART_RX_GPIO_Port, &GPIO_InitStructure);

    /* Status LEDs PB0/PB1 + APA102 CLK/DATA on PC0/PC1: output push-pull */
    GPIO_InitStructure.GPIO_Pin = LED1_Pin | LED2_Pin;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_Low;
    GPIO_Init(LED1_GPIO_Port, &GPIO_InitStructure);
    GPIO_InitStructure.GPIO_Pin = LED_CLK_Pin | LED_DO_Pin;
    GPIO_Init(LED_CLK_GPIO_Port, &GPIO_InitStructure);
    /* CLK and DATA idle low */
    LED_CLK_GPIO_Port->BCR = LED_CLK_Pin | LED_DO_Pin;

    /* EXTI6: PC6 (CE1/UDS), falling edge */
    GPIO_EXTILineConfig(GPIO_PortSourceGPIOC, GPIO_PinSource6);
    EXTI_InitStructure.EXTI_Line = EXTI_Line6;
    EXTI_InitStructure.EXTI_Mode = EXTI_Mode_Interrupt;
    EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Falling;
    EXTI_InitStructure.EXTI_LineCmd = ENABLE;
    EXTI_Init(&EXTI_InitStructure);

    /* EXTI9: PC9 (CE2/LDS), falling edge */
    GPIO_EXTILineConfig(GPIO_PortSourceGPIOC, GPIO_PinSource9);
    EXTI_InitStructure.EXTI_Line = EXTI_Line9;
    EXTI_InitStructure.EXTI_Mode = EXTI_Mode_Interrupt;
    EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Falling;
    EXTI_InitStructure.EXTI_LineCmd = ENABLE;
    EXTI_Init(&EXTI_InitStructure);

    /* EXTI15: PA15 (RESET), falling edge */
    GPIO_EXTILineConfig(GPIO_PortSourceGPIOA, GPIO_PinSource15);
    EXTI_InitStructure.EXTI_Line = EXTI_Line15;
    EXTI_InitStructure.EXTI_Mode = EXTI_Mode_Interrupt;
    EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Falling;
    EXTI_InitStructure.EXTI_LineCmd = ENABLE;
    EXTI_Init(&EXTI_InitStructure);
}
