#ifndef __PCMCIA_H
#define __PCMCIA_H

#include "debug.h"

/* GPIOB control signal bit masks */
#define UDS_MASK    0x0040u  /* PB6  - CE1/UDS (active LOW) */
#define OE_MASK     0x0080u  /* PB7  - OE      (active LOW) */
#define WE_MASK     0x0100u  /* PB8  - WE      (active LOW) */
#define LDS_MASK    0x0200u  /* PB9  - CE2/LDS (active LOW) */
#define REG_MASK    0x0400u  /* PB10 - REG     (active LOW) */
#define IOR_MASK    0x0800u  /* PB11 - IOR     (active LOW) */
#define IOW_MASK    0x1000u  /* PB12 - IOW     (active LOW) */
#define RESET_MASK  0x2000u  /* PB13 - RESET   (active LOW) */

/* Both strobes: wait until both go HIGH before floating data bus */
#define STROBE_MASK (UDS_MASK | LDS_MASK)
#define MEM_MASK (OE_MASK | WE_MASK)
#define IO_MASK (IOR_MASK | IOW_MASK)
#define ACCESS_MASK (MEM_MASK | IO_MASK)

/* Gayle swaps PCMCIA D0-D7 <-> D8-D15 relative to the 68000 bus.
 * Apply to every 16-bit value written to GPIOD->OUTDR. */
#define BUS16(v) ((((v) & 0xFF) << 8) | (((v) >> 8) & 0xFF))

extern volatile uint32_t pcmcia_irq_count;
extern volatile uint32_t pcmcia_mem_count;
extern volatile uint32_t pcmcia_io_count;


void Init_PCMCIA(void);
void PCMCIA_Handler(void);

#endif /* __PCMCIA_H */
