#ifndef __PCMCIA_H
#define __PCMCIA_H

#include "debug.h"

/* GPIOC control signal bit masks — same bit positions as the old GPIOB
 * layout (PB6..PB12 -> PC6..PC12), so the values are unchanged. */
#define UDS_MASK    0x0040u  /* PC6  - CE1/UDS (active LOW), EXTI6 */
#define OE_MASK     0x0080u  /* PC7  - OE      (active LOW) */
#define WE_MASK     0x0100u  /* PC8  - WE      (active LOW) */
#define LDS_MASK    0x0200u  /* PC9  - CE2/LDS (active LOW), EXTI9 */
#define REG_MASK    0x0400u  /* PC10 - REG     (active LOW) */
#define IOR_MASK    0x0800u  /* PC11 - IOR     (active LOW) */
#define IOW_MASK    0x1000u  /* PC12 - IOW     (active LOW) */
/* RESET is PA15 (EXTI15) and is never read in the hot path. */

/* Both strobes: wait until both go HIGH before floating data bus */
#define STROBE_MASK (UDS_MASK | LDS_MASK)
#define MEM_MASK (OE_MASK | WE_MASK)
#define IO_MASK (IOR_MASK | IOW_MASK)
#define ACCESS_MASK (MEM_MASK | IO_MASK)

/* 22-bit address reassembly (PE7 does not exist on the CH32V467):
 *   PE0-PE6  -> A0-A6     (in place)
 *   PE8-PE11 -> A8-A11    (in place)
 *   PB3      -> A7        (same <<4 shift as the run below)
 *   PB8-PB15 -> A12-A19
 *   PB4,PB5  -> A20,A21                                            */
#define ADDR_E_MASK   0x00000F7Fu  /* PE bits 0-6, 8-11               */
#define ADDR_B1_MASK  0x000FF080u  /* after <<4  : A7, A12-A19        */
#define ADDR_B2_MASK  0x00300000u  /* after <<16 : A20, A21           */
#define PCMCIA_ADDR(e, b) (((e) & ADDR_E_MASK) | \
                           (((b) << 4)  & ADDR_B1_MASK) | \
                           (((b) << 16) & ADDR_B2_MASK))

/* Gayle swaps PCMCIA D0-D7 <-> D8-D15 relative to the 68000 bus.
 * Apply to every 16-bit value written to GPIOD->OUTDR.
 * An inline function, not a macro: a macro evaluates its argument twice,
 * which for a volatile operand (GPIOD->INDR, a PSRAM load) emits two bus
 * reads back to back on the ISR hot path. */
static __attribute__((always_inline)) inline uint16_t BUS16(uint16_t v) {
    return (uint16_t)((v << 8) | (v >> 8));
}

#define PCMCIA_NUM_LEDS 8u

extern volatile uint32_t pcmcia_irq_count;
extern volatile uint32_t pcmcia_mem_count;
extern volatile uint32_t pcmcia_io_count;

extern uint32_t led_fb[PCMCIA_NUM_LEDS];
uint8_t PCMCIA_BoardCtrl(void);

/* Handler outcome counters, readable over serial / debugger */
enum { PCMCIA_ST_GLITCH, PCMCIA_ST_GLITCH2, PCMCIA_ST_ATTR, PCMCIA_ST_TIMEOUT,
       PCMCIA_ST_MEM_RD, PCMCIA_ST_MEM_WR, PCMCIA_ST_IO_WR, PCMCIA_ST_IO_RD,
       PCMCIA_ST_RESET, PCMCIA_ST_COUNT };
extern volatile uint32_t pcmcia_stats[PCMCIA_ST_COUNT];

/* Bus trace + counter printout on USART1: build with `make TRACE=1`. */
#ifndef PCMCIA_TRACE
#define PCMCIA_TRACE 0
#endif

#if PCMCIA_TRACE
/* First cycles after each Amiga reset: addr | kind<<24, ctrl lines, GPIOD->OUTDR */
#define PCMCIA_TRACE_LEN 128u
typedef struct { uint32_t addr; uint16_t ctrl; uint16_t data; } PcmciaTrace;
extern volatile PcmciaTrace pcmcia_trace[PCMCIA_TRACE_LEN];
extern volatile uint32_t pcmcia_trace_idx;
#endif

void PCMCIA_Init(void);
void PCMCIA_Handler(void);
void PCMCIA_ResetHandler(void);
void PCMCIA_PollLoop(void);


#endif /* __PCMCIA_H */
