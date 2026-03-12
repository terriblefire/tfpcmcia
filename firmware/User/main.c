#include "debug.h"
#include "gpio.h"
#include "pcmcia.h"

int main(void) {
    SystemCoreClockUpdate();
    Delay_Init();
    GPIO_Config();
    USART1_Init(115200);
    Init_PCMCIA();

    printf("tfpcmcia v0.1 ready\r\n");
    printf("BEEF/BABE test firmware — waiting for PCMCIA access\r\n");

    uint32_t last_irq = 0;

    while (1) {
        Delay_Ms(500);
        GPIOC->OUTDR ^= GPIO_Pin_8;

        uint32_t irq = pcmcia_irq_count;
        if (irq != last_irq) {
            printf("irq=%lu mem=%lu io=%lu\r\n",
                   pcmcia_irq_count, pcmcia_mem_count, pcmcia_io_count);
            last_irq = irq;
        }
    }
}
