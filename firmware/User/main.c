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
        Delay_Ms(1);
    }

    //PCMCIA_PollLoop();
}
