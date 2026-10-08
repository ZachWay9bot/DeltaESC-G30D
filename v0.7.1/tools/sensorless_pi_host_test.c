#include <stdint.h>
#include <stdio.h>
#include "sensorless_control.h"

#define LIMIT_Q15 (2600 * 32768)

static int check_small_error(void){
    sensorless_control_t s;
    phase_current_counts_t z={0,0,0};
    sensorless_control_init(&s);
    sensorless_control_step(&s,z,2500u,1999u,0u);
    if(s.ccr1!=1000u||s.ccr2!=1000u||s.ccr3!=1000u)return 1;
    sensorless_control_set_drive(&s,1u);
    for(unsigned k=1;k<=400u;k++){
        sensorless_control_step(&s,z,2500u,1999u,1u);
        if(s.state!=SENSORLESS_ALIGN)return 2;
        if(s.iq_int!=(int32_t)(k*10u*220u))return 3;
        if(s.id_int!=0)return 4;
        if(s.ccr1>1999u||s.ccr2>1999u||s.ccr3>1999u)return 5;
    }
    if(s.iq_int!=880000)return 6;
    printf("Q15 integrator 400 ticks: %ld (expected 880000)\n",(long)s.iq_int);
    sensorless_control_stop(&s);
    if(s.id_int||s.iq_int)return 7;
    sensorless_control_step(&s,z,2500u,1999u,0u);
    if(s.state!=SENSORLESS_STOP||s.ccr1!=1000u||s.ccr2!=1000u||s.ccr3!=1000u)return 8;
    return 0;
}
static int check_clamping(void){
    sensorless_control_t s;sensorless_control_init(&s);sensorless_control_set_drive(&s,1u);
    s.iq_int=LIMIT_Q15-1000;
    sensorless_control_step(&s,(phase_current_counts_t){0,0,0},2500u,1999u,1u);
    if(s.iq_int!=LIMIT_Q15)return 9;
    s.id_int=-LIMIT_Q15;
    sensorless_control_step(&s,(phase_current_counts_t){10,0,-10},2500u,1999u,1u);
    if(s.id_int< -LIMIT_Q15 || s.id_int>LIMIT_Q15)return 10;
    return 0;
}
static int check_stress(void){
    sensorless_control_t s;sensorless_control_init(&s);sensorless_control_set_drive(&s,1u);
    for(unsigned k=0;k<10000u;k++){
        int16_t i=(int16_t)(((k*173u)%301u)-150);
        sensorless_control_step(&s,(phase_current_counts_t){i,(int16_t)(i/2),(int16_t)-i},
                               (uint16_t)(1700u+k%300u),1999u,1u);
        if(s.ccr1>1999u||s.ccr2>1999u||s.ccr3>1999u)return 11;
        if(s.iq_int<-LIMIT_Q15||s.iq_int>LIMIT_Q15||s.id_int<-LIMIT_Q15||s.id_int>LIMIT_Q15)return 12;
        if(s.state==SENSORLESS_FAULT){sensorless_control_stop(&s);sensorless_control_set_drive(&s,1u);}
    }
    return 0;
}
int main(void){
    int r=check_small_error();if(r)return r;
    r=check_clamping();if(r)return r;
    r=check_stress();if(r)return r;
    puts("PASS Q15 small-error, saturation, PWM bounds, coast and stop");
    return 0;
}
