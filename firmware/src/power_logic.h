#pragma once
#include <stdint.h>
typedef struct { uint16_t held_ms; uint16_t threshold_ms; uint8_t fired; } power_button_state_t;
void power_button_init(power_button_state_t *s,uint16_t threshold_ms);
uint8_t power_button_step(power_button_state_t *s,uint8_t pressed_sample,uint8_t inhibit);
