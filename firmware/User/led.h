#ifndef __LED_H
#define __LED_H

#include <stdint.h>

void APA102_White(uint16_t num_leds);
void APA102_Kitt(uint16_t frame);
void APA102_FB(const uint32_t *pixels);

#endif
