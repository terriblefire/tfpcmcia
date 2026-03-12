#ifndef __DEBUG_H
#define __DEBUG_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stdio.h"
#include "ch32v30x.h"

void Delay_Init(void);
void Delay_Us(uint32_t n);
void Delay_Ms(uint32_t n);
void USART1_Init(uint32_t baudrate);

#ifdef __cplusplus
}
#endif

#endif /* __DEBUG_H */
