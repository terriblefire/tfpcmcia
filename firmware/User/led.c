#include "led.h"
#include "gpio.h"

#define CLK_H() (LED_CLK_GPIO_Port->BSHR = LED_CLK_Pin)
#define CLK_L() (LED_CLK_GPIO_Port->BCR  = LED_CLK_Pin)
#define DAT_H() (LED_DO_GPIO_Port->BSHR  = LED_DO_Pin)
#define DAT_L() (LED_DO_GPIO_Port->BCR   = LED_DO_Pin)

static void send_byte(uint8_t b)
{
    for (int i = 7; i >= 0; i--) {
        if (b & (1 << i)) DAT_H(); else DAT_L();
        CLK_H();
        CLK_L();
    }
}

static void send_u32(uint32_t v)
{
    send_byte(v >> 24); send_byte(v >> 16);
    send_byte(v >> 8);  send_byte(v);
}

#define KITT_NUM_LEDS  7
#define KITT_FRAMES    1024
#define KITT_STEPS     12   /* 2*(NUM_LEDS-1) */

static uint8_t kitt_br(int dist)
{
    switch (dist) {
        case 0:  return 31;
        case 1:  return 8;
        case 2:  return 2;
        default: return 0;
    }
}

void APA102_Kitt(uint16_t frame)
{
    uint16_t step = (uint16_t)((uint32_t)(frame % KITT_FRAMES) * KITT_STEPS / KITT_FRAMES);
    int pos = (step <= 6) ? (int)step : (int)(KITT_STEPS - step);

    send_u32(0x00000000);               /* start frame */
    for (int i = 0; i < KITT_NUM_LEDS; i++) {
        int d = i - pos;
        if (d < 0) d = -d;
        uint8_t br = kitt_br(d);
        send_byte(0xE0 | br);           /* global brightness */
        send_byte(0x00);                /* blue  */
        send_byte(0x00);                /* green */
        send_byte(br ? 0xFF : 0x00);   /* red — off when br==0 */
    }
    send_u32(0xFFFFFFFF);               /* end frame */
}

void APA102_FB(const uint32_t *pixels)
{
    send_u32(0x00000000);               /* start frame */
    for (int i = 0; i < 7; i++) {
        uint8_t r = (uint8_t)(pixels[i] >> 24);
        uint8_t g = (uint8_t)(pixels[i] >> 16);
        uint8_t b = (uint8_t)(pixels[i] >>  8);
        send_byte(0xFF);                /* global brightness = max */
        send_byte(b);
        send_byte(g);
        send_byte(r);
    }
    send_u32(0xFFFFFFFF);               /* end frame */
}

void APA102_White(uint16_t num_leds)
{
    send_u32(0x00000000);                   /* start frame */
    for (uint16_t i = 0; i < num_leds; i++)
        send_u32(0xFFFFFFFF);               /* brightness=31, B=255, G=255, R=255 */
    send_u32(0xFFFFFFFF);                   /* end frame */
}
