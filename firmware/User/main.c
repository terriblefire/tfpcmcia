#include "debug.h"
#include "gpio.h"
#include "pcmcia.h"


int main(void) {
    
    SystemCoreClockUpdate();
    Delay_Init();
    GPIO_Config();
    USART1_Init(115200);
    PCMCIA_Init();

    printf("tfpcmcia v0.2ready\r\n");
    printf("Testing sdcard\r\n");

    LED2_GPIO_Port->OUTDR ^= LED2_Pin;
    uint16_t ctrl;

    while (1) {

        Delay_Ms(1);


    }
}
