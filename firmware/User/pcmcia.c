#include "pcmcia.h"

volatile uint32_t pcmcia_irq_count = 0;
volatile uint32_t pcmcia_mem_count = 0;
volatile uint32_t pcmcia_io_count  = 0;

/* Fast interrupt handler: runs from VTF (Vector Table Fast) entry. */
void PCMCIA_Handler(void) __attribute__((interrupt("WCH-Interrupt-fast")));

void PCMCIA_Handler(void) {
    EXTI->INTFR = EXTI_Line6 | EXTI_Line9;

    pcmcia_irq_count++;

    uint16_t ctrl = (uint16_t)GPIOB->INDR;

    /* IO read: IOR low — check before memory so IO cycles aren't misread.
     * IOR goes low simultaneously with CE, so no timing ambiguity here. */
    if ((ctrl & IOR_MASK) == 0) {
        pcmcia_io_count++;
        GPIOD->OUTDR = BUS16(0xBABE);
        GPIOD->CFGLR = 0x33333333;
        GPIOD->CFGHR = 0x33333333;
        while ((GPIOB->INDR & STROBE_MASK) != STROBE_MASK) {}
        GPIOD->CFGLR = 0x44444444;
        GPIOD->CFGHR = 0x44444444;
        return;
    }

    /* Memory read: WE high = not a write.
     * REG low = attribute memory, REG high = common memory. */
    if ((ctrl & WE_MASK) != 0) {
        pcmcia_mem_count++;
        uint16_t val = ((ctrl & REG_MASK) == 0) ? 0xBABE : 0xBEEF;
        GPIOD->OUTDR = BUS16(val);
        GPIOD->CFGLR = 0x33333333;
        GPIOD->CFGHR = 0x33333333;
        while ((GPIOB->INDR & STROBE_MASK) != STROBE_MASK) {}
        GPIOD->CFGLR = 0x44444444;
        GPIOD->CFGHR = 0x44444444;
        return;
    }
}

void Init_PCMCIA(void) {
    GPIOB->BSHR = GPIO_Pin_14;
    GPIOA->BSHR = GPIO_Pin_2;

    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);
    SetVTFIRQ((u32)PCMCIA_Handler, EXTI9_5_IRQn, 0, ENABLE);
    NVIC_SetPriority(EXTI9_5_IRQn, 0);
    NVIC_EnableIRQ(EXTI9_5_IRQn);
}
