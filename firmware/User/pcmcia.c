#include "pcmcia.h"

volatile uint32_t pcmcia_irq_count = 0;
volatile uint32_t pcmcia_mem_count = 0;
volatile uint32_t pcmcia_io_count  = 0;

/* ---- Dispatch functions -------------------------------------------------
 * Must be always_inline: PCMCIA_Handler uses WCH-Interrupt-fast which does
 * not save/restore caller-saved registers. Calling a normal function from
 * such a handler corrupts a0-a7, t0-t6, ra — producing garbage results.
 * always_inline eliminates the call entirely. */

static __attribute__((always_inline)) uint16_t PCMCIA_Memory_Read(uint32_t addr) {
    (void)addr;
    return 0xBEEF;
}

static __attribute__((always_inline)) uint16_t PCMCIA_Reg_Read(uint32_t addr) {
    return (addr & 0x2) ? (addr & 0xFFFC) : (addr >> 16) & 0xFFFF;
}

static __attribute__((always_inline)) void PCMCIA_Memory_Write(uint32_t addr, uint16_t data) {
    (void)addr;
    (void)data;
}

static __attribute__((always_inline)) void PCMCIA_Reg_Write(uint32_t addr, uint16_t data) {
    (void)addr;
    (void)data;
}

/* ---- Interrupt handler -------------------------------------------------- */

void PCMCIA_Handler(void) __attribute__((interrupt("WCH-Interrupt-fast")));

void PCMCIA_Handler(void) {
    EXTI->INTFR = EXTI_Line6 | EXTI_Line9;
    pcmcia_irq_count++;

    uint16_t ctrl = (uint16_t)GPIOB->INDR;
    uint32_t addr  = ((uint32_t)(ctrl & 0x003Fu) << 16) | (uint16_t)GPIOE->INDR;

    /* IOR: IO read cycle — no dispatch function yet */
    if ((ctrl & IOR_MASK) == 0) {
        pcmcia_io_count++;
        return;
    }

    if ((ctrl & WE_MASK) != 0) {
        /* Read cycle: REG low = attribute memory, REG high = common memory */
        pcmcia_mem_count++;
        uint16_t val = ((ctrl & REG_MASK) == 0)
                       ? PCMCIA_Reg_Read(addr)
                       : PCMCIA_Memory_Read(addr);
        GPIOD->OUTDR = BUS16(val);
        GPIOD->CFGLR = 0x33333333;
        GPIOD->CFGHR = 0x33333333;
        while ((GPIOB->INDR & STROBE_MASK) != STROBE_MASK) {}
        GPIOD->CFGLR = 0x44444444;
        GPIOD->CFGHR = 0x44444444;
    } else {
        /* Write cycle: GPIOD stays floating; read data the Amiga is driving */
        uint16_t data = BUS16((uint16_t)GPIOD->INDR);
        if ((ctrl & REG_MASK) == 0)
            PCMCIA_Reg_Write(addr, data);
        else
            PCMCIA_Memory_Write(addr, data);
    }
}

/* ---- Init --------------------------------------------------------------- */

void Init_PCMCIA(void) {
    GPIOB->BSHR = GPIO_Pin_14;
    GPIOA->BSHR = GPIO_Pin_2;

    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);
    SetVTFIRQ((u32)PCMCIA_Handler, EXTI9_5_IRQn, 0, ENABLE);
    NVIC_SetPriority(EXTI9_5_IRQn, 0);
    NVIC_EnableIRQ(EXTI9_5_IRQn);
}
