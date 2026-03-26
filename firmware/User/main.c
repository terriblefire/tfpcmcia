#include "debug.h"
#include "gpio.h"
#include "pcmcia.h"


int main(void) {

    /* Drive /WAIT (PB14) and /READY (PA2) low ASAP — before any HAL init.
     * Enable port clocks, configure as output push-pull, then drive low. */
    RCC->APB2PCENR |= RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOB;
    /* PB14: set CFGHR bits [27:24] = 0x3 (output 50MHz push-pull) */
    GPIOB->CFGHR = (GPIOB->CFGHR & ~(0xFu << 24)) | (0x3u << 24);
    GPIOB->BCR = GPIO_Pin_14;   /* /WAIT low */
    /* PA2: set CFGLR bits [11:8] = 0x3 (output 50MHz push-pull) */
    GPIOA->CFGLR = (GPIOA->CFGLR & ~(0xFu << 8)) | (0x3u << 8);
    GPIOA->BCR = GPIO_Pin_2;    /* /READY low */

    SystemCoreClockUpdate();
    Delay_Init();
    GPIO_Config();
    USART1_Init(115200);
    PCMCIA_Init();

    printf("tfpcmcia v0.3ready\r\n");
    printf("Testing sdcard\r\n");

    LED2_GPIO_Port->OUTDR ^= LED2_Pin;
    uint16_t ctrl;


    while (1)
    {
        Delay_Ms(10);
    }

    //PCMCIA_PollLoop();
}
