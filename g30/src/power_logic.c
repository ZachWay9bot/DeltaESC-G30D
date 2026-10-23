#include "power_logic.h"
void power_button_init(power_button_state_t *s,uint16_t threshold_ms){
    s->held_ms=0u;s->threshold_ms=threshold_ms;s->fired=0u;
}
uint8_t power_button_step(power_button_state_t *s,uint8_t pressed_sample,uint8_t inhibit){
    if(inhibit||!pressed_sample){s->held_ms=0u;s->fired=0u;return 0u;}
    if(s->held_ms<s->threshold_ms)s->held_ms++;
    if(!s->fired&&s->held_ms>=s->threshold_ms){s->fired=1u;return 1u;}
    return 0u;
}
