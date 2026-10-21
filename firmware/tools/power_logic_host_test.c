#include <stdint.h>
#include "power_logic.h"
int main(void){
    power_button_state_t s;power_button_init(&s,6000u);
    for(unsigned i=0;i<5999u;i++)if(power_button_step(&s,1u,0u))return 1;
    if(!power_button_step(&s,1u,0u))return 2;
    for(unsigned i=0;i<10000u;i++)if(power_button_step(&s,1u,0u))return 3;
    if(power_button_step(&s,0u,0u))return 4;
    for(unsigned i=0;i<9000u;i++)if(power_button_step(&s,1u,1u))return 5;
    if(s.held_ms||s.fired)return 6;
    for(unsigned i=0;i<5999u;i++)if(power_button_step(&s,1u,0u))return 7;
    if(!power_button_step(&s,1u,0u))return 8;
    if(power_button_step(&s,1u,1u))return 9;
    return 0;
}
