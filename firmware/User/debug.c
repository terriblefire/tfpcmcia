#include "debug.h"

#if defined(__GNUC__)
#pragma GCC push_options
#pragma GCC optimize("Os")
#endif

static uint8_t  p_us = 0;
static uint16_t p_ms = 0;

void Delay_Init(void) {
    p_us = SystemCoreClock / 8000000;
    p_ms = (uint16_t)p_us * 1000;
}

void Delay_Us(uint32_t n) {
    uint32_t i;
    SysTick->SR &= ~(1 << 0);
    i = (uint32_t)n * p_us;
    SysTick->CMP = i;
    SysTick->CTLR |= (1 << 4);
    SysTick->CTLR |= (1 << 5) | (1 << 0);
    while ((SysTick->SR & (1 << 0)) != (1 << 0))
        ;
    SysTick->CTLR &= ~(1 << 0);
}

void Delay_Ms(uint32_t n) {
    uint32_t i;
    SysTick->SR &= ~(1 << 0);
    i = (uint32_t)n * p_ms;
    SysTick->CMP = i;
    SysTick->CTLR |= (1 << 4);
    SysTick->CTLR |= (1 << 5) | (1 << 0);
    while ((SysTick->SR & (1 << 0)) != (1 << 0))
        ;
    SysTick->CTLR &= ~(1 << 0);
}

/* USART1 on PA9 (TX) / PA10 (RX) — GPIO already configured in gpio.c */
void USART1_Init(uint32_t baudrate) {
    USART_InitTypeDef USART_InitStructure = {0};

    RCC->APB2PCENR |= RCC_APB2Periph_USART1;

    USART_InitStructure.USART_BaudRate            = baudrate;
    USART_InitStructure.USART_WordLength          = USART_WordLength_8b;
    USART_InitStructure.USART_StopBits            = USART_StopBits_1;
    USART_InitStructure.USART_Parity              = USART_Parity_No;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode                = USART_Mode_Tx | USART_Mode_Rx;
    USART_Init(USART1, &USART_InitStructure);
    USART_Cmd(USART1, ENABLE);
}

/* Retarget printf to USART1 */
int _write(int fd, char *buf, int size) {
    (void)fd;
    for (int i = 0; i < size; i++) {
        while (USART_GetFlagStatus(USART1, USART_FLAG_TC) == RESET)
            ;
        USART_SendData(USART1, (uint8_t)buf[i]);
    }
    return size;
}

#if defined(__GNUC__)
#pragma GCC pop_options
#endif
