#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include "sensorless_control.h"

static int16_t wrapdiff(uint16_t a,uint16_t b){return (int16_t)(a-b);}
static uint16_t atan_u16(int32_t y,int32_t x){
    double a=atan2((double)y,(double)x);
    if(a<0)a+=2.0*3.14159265358979323846;
    return (uint16_t)(a*(65536.0/(2.0*3.14159265358979323846))+0.5);
}
int main(void){
    sensorless_control_t s;
    sensorless_control_init(&s);
    assert(sensorless_control_set_motor_params(&s,583000u,640000u,12100u));
    assert(sensorless_control_set_run_current_ma(&s,600u));
    sensorless_control_set_phase_offset(&s,(int16_t)21327);
    const uint16_t control_phase=50000u;
    assert(sensorless_control_start_rolling(&s,control_phase,-180));
    assert(s.state==SENSORLESS_ROLL_VERIFY);
    assert(s.observer_phase==control_phase);
    const uint16_t raw=atan_u16(s.vesc_flux.flux_beta_nwb,s.vesc_flux.flux_alpha_nwb);
    const uint16_t expected=(uint16_t)(control_phase-21327u);
    int16_t d=wrapdiff(raw,expected); if(d<0)d=(int16_t)-d;
    assert((uint16_t)d<400u);
    assert(s.rolling_step==-180);
    puts("PASS rolling frame seed: FOC phase keeps +117.15deg frame offset while observer flux seed remains physical alpha/beta");
    return 0;
}
