#include "gpio.h"


static void GPIO_Spi_Init(SPI_TypeDef *spi, GPIO_TypeDef *gpio, uint16_t pins, uint16_t prescaler) {

    GPIO_InitTypeDef GPIO_InitStructure = {0};
    SPI_InitTypeDef SPI_InitStructure = {0};

    // SCK, MISO, MOSI pin configuration
    GPIO_InitStructure.GPIO_Pin = pins;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
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
    RCC->APB2PCENR |= RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOB |
                      RCC_APB2Periph_GPIOC | RCC_APB2Periph_GPIOD |
                      RCC_APB2Periph_GPIOE | RCC_APB2Periph_AFIO;

    /* Address bus A0-A15 on GPIOE: input floating */
    GPIOE->CFGLR = 0x44444444;
    GPIOE->CFGHR = 0x44444444;

    /* Data bus D0-D15 on GPIOD: input floating (high-Z, tristate) */
    GPIOD->CFGLR = 0x44444444;
    GPIOD->CFGHR = 0x44444444;

    /* PB0-PB5 (A16-A21): input floating */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_2 |
                                  GPIO_Pin_3 | GPIO_Pin_4 | GPIO_Pin_5;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    /* PB6 (CE1/UDS), PB7 (OE), PB8 (WE), PB9 (CE2/LDS),
       PB10 (REG), PB11 (IOR), PB12 (IOW), PB13 (RESET): input pull-up */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6 | GPIO_Pin_7 | GPIO_Pin_8 |
                                  GPIO_Pin_9 | GPIO_Pin_10 | GPIO_Pin_11 |
                                  GPIO_Pin_12 | GPIO_Pin_13;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    /* PB14 (WAIT): output push-pull, drive HIGH (no wait states) */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_14;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &GPIO_InitStructure);
    GPIOB->BSHR = GPIO_Pin_14;   /* WAIT = HIGH */

    /* PB15 (IOIS16): output push-pull (LOW = 16-bit port) */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_15;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &GPIO_InitStructure);
    GPIOB->BCR = GPIO_Pin_15;    /* IOIS16 = LOW (16-bit) */

    /* PA2 (READY): output push-pull, drive HIGH (card ready) */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);   /* READY = HIGH */

    /* PA1 (SPIRAM CS): output push-pull, drive HIGH (deasserted) */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_1;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_Init(GPIOA, &GPIO_InitStructure);
    GPIO_WriteBit(GPIOA, GPIO_Pin_1, Bit_SET);
    RCC_APB2PeriphClockCmd (RCC_APB2Periph_SPI1, ENABLE);
    GPIO_Spi_Init(SPI1, GPIOA, GPIO_Pin_5 | GPIO_Pin_6 | GPIO_Pin_7, SPI_BaudRatePrescaler_4);  /* SCK, MISO, MOSI */

   
    /* PA9 (USART1_TX): alternate function push-pull */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    /* PA10 (USART1_RX): input floating */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    /* LEDx: output push-pull */
    GPIO_InitStructure.GPIO_Pin = LED1_Pin | LED2_Pin;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOC, &GPIO_InitStructure);

    /* EXTI6: PB6 (UDS/CE1), falling edge */
    GPIO_EXTILineConfig(GPIO_PortSourceGPIOB, GPIO_PinSource6);
    EXTI_InitStructure.EXTI_Line = EXTI_Line6;
    EXTI_InitStructure.EXTI_Mode = EXTI_Mode_Interrupt;
    EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Falling;
    EXTI_InitStructure.EXTI_LineCmd = ENABLE;
    EXTI_Init(&EXTI_InitStructure);

    /* EXTI9: PB9 (LDS/CE2), falling edge */
    GPIO_EXTILineConfig(GPIO_PortSourceGPIOB, GPIO_PinSource9);
    EXTI_InitStructure.EXTI_Line = EXTI_Line9;
    EXTI_InitStructure.EXTI_Mode = EXTI_Mode_Interrupt;
    EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Falling;
    EXTI_InitStructure.EXTI_LineCmd = ENABLE;
    EXTI_Init(&EXTI_InitStructure);

    /* PA15 (SD CD): input pull-up (LOW = card inserted) */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_15;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

     /* PC9 (SD CS): output push-pull, drive HIGH (deasserted) */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOC, &GPIO_InitStructure);
    GPIO_WriteBit(GPIOC, GPIO_Pin_9, Bit_SET);

    /* SPI3 remap: PC10=SCK, PC11=MISO, PC12=MOSI */
    GPIO_PinRemapConfig(GPIO_Remap_SPI3, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_SPI3, ENABLE);
    GPIO_Spi_Init(SPI3, GPIOC, GPIO_Pin_10 | GPIO_Pin_12, SPI_BaudRatePrescaler_8);  /* SCK, MOSI as AF_PP */
    /* MISO as input pull-up — holds MISO high when SD card releases the bus */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_11;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOC, &GPIO_InitStructure);
}
