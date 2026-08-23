#include "psram.h"
#include "psram_wch.h"
#include "debug.h"

/* Bring up the on-chip PSRAM controller. Follows the WCH PSRAM example
 * (EVT/EXAM/PSRAM), retimed for our 200 MHz PSRAM clock (= SYSCLK):
 *   TRC  = 0x0C : 12 cycles * 5 ns = 60 ns, the tRC minimum. The RM's
 *                 own examples interpolate exactly (333M->0x14, 166M->0x0A).
 *   TCPH = 0x0C : reset default, satisfies tCPH at any clock <= 333 MHz.
 *   TXLPD= 0x07 : counts HSI cycles, clock-independent (reset default).
 * Latency uses the 200M mode-register values. Read latency is FIXED:
 * variable-latency read is a good-lot-only feature (DS Table 3-42 note),
 * fixed works on every lot. Switch to Read_Variable on confirmed lots
 * for lower average read latency. */
void PSRAM_Init(void) {
    PSRAMInitTypeDef PSRAMInitStruct = {0};
    PSRAMTimingInitTypeDef PSRAMTimingStruct = {0};

    RCC_HBPeriphClockCmd(RCC_HBPeriph_PSRAM, ENABLE);

    PSRAMDeInit();

    PSRAMTimingStruct.PSRAM_trc   = 0x0C;
    PSRAMTimingStruct.PSRAM_tcph  = 0x0C;
    PSRAMTimingStruct.PSRAM_txlpd = 0x07;
    PSRAMInitStruct.PSRAM_cfifo     = PSRAM_CFIFO_BTWWRRD;
    PSRAMInitStruct.PSRAM_cap_cfg   = PSRAM_CAP_64M;   /* VET6 = 8 MB */
    PSRAMInitStruct.PSRAM_exti_lpmd = PSRAM_EXIT_LPMD;
    PSRAMInitStruct.PSRAMTimingStruct = &PSRAMTimingStruct;
    PSRAMInit(&PSRAMInitStruct);
    Delay_Ms(1);

    SetWrLatency(MR4_Write_200M, Latency_200M);
    SetRdLatency(MR0_Read_200M, Latency_200M, Read_Fixed);
}

/* Non-destructive probe: save the first word, walk a few patterns
 * through it, restore. Returns 1 if every pattern reads back. */
int PSRAM_Probe(void) {
    static const uint16_t pat[] = { 0xA55Au, 0x5AA5u, 0x0001u, 0xFFFEu };
    uint16_t saved = PSRAM_Read16(0);
    int ok = 1;

    for (unsigned i = 0; i < sizeof(pat) / sizeof(pat[0]); i++) {
        PSRAM_Write16(0, pat[i]);
        if (PSRAM_Read16(0) != pat[i]) {
            ok = 0;
            break;
        }
    }

    PSRAM_Write16(0, saved);
    return ok;
}
