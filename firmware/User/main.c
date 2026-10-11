#include "debug.h"
#include "gpio.h"
#include "led.h"
#include "pcmcia.h"
#include "psram.h"


int main(void) {

    SystemCoreClockUpdate();
    Delay_Init();
    GPIO_Config();
    USART1_Init(115200);
    PCMCIA_Init();

    printf("tfpcmcia v0.4 (CH32V467) ready\r\n");

    /* Probe printed AFTER the banner: if the 0x8000_0000 window is not
     * accessible out of reset this access hangs or faults, and the last
     * UART line tells us exactly where. */
    PSRAM_Init();
    printf("PSRAM probe: ");
    printf(PSRAM_Probe() ? "OK\r\n" : "FAILED\r\n");

    RCC->PB1PCENR |= RCC_PB1Periph_PWR;
    printf("VDD18 level %lu, VDDK level %lu\r\n",
           (EXTEN->EXTEN_CTR & EXTEN_LDO18_ADJ) ? 1UL : 0UL,
           (PWR->CTLR & PWR_CTLR_LDOVDDK) >> 24);

    LED2_GPIO_Port->OUTDR ^= LED2_Pin;

    uint16_t frame = 0;
    uint16_t last_ctrl = 0xFFFF;
    while (1)
    {
        uint16_t ctrl = PCMCIA_BoardCtrl() & 0x03;
        if (ctrl != last_ctrl) {
            frame = 0;
            last_ctrl = ctrl;
        }
        switch (ctrl) {
            case 0:  APA102_GreenRamp(frame); break;
            case 1:  APA102_Kitt(frame);      break;
            case 2:  APA102_AmberBlink(frame); break;
            case 3:  APA102_FB(led_fb);       break;
            default: break;
        }
        frame++;
#if PCMCIA_TRACE
        if ((frame % 1000) == 0) {
            static uint32_t last[PCMCIA_ST_COUNT];
            uint32_t now[PCMCIA_ST_COUNT], changed = 0;
            for (int i = 0; i < PCMCIA_ST_COUNT; i++) {
                now[i] = pcmcia_stats[i];
                changed |= now[i] != last[i];
                last[i] = now[i];
            }
            if (changed)
                printf("glitch %lu/%lu attr %lu tmo %lu mrd %lu mwr %lu iow %lu ior %lu rst %lu ctrl %u\r\n",
                       now[0], now[1], now[2], now[3], now[4], now[5], now[6], now[7], now[8], ctrl);
        }
        {
            static uint32_t printed;
            uint32_t n = pcmcia_trace_idx;
            if (n < printed) {
                printf("-- trace restart (reset) --\r\n");
                printed = 0;
            }
            /* gap = ns since previous handler entry, isr = ns entry->exit */
            for (int k = 0; k < 4 && printed < n; k++, printed++) {
                PcmciaTrace t = pcmcia_trace[printed];
                uint32_t gap = printed ? (t.t0 - pcmcia_trace[printed - 1].t0) * 5 : 0;
                printf("T%03lu k%lu a=%06lx c=%04x d=%04x gap=%lu isr=%lu\r\n", printed,
                       t.addr >> 24, t.addr & 0x3FFFFF, t.ctrl, t.data,
                       gap, (t.t1 - t.t0) * 5);
            }
        }
#endif
        Delay_Ms(1);
    }

    //PCMCIA_PollLoop();
}
