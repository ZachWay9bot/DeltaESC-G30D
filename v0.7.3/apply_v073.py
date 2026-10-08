#!/usr/bin/env python3
from pathlib import Path

root=Path(__file__).resolve().parent
c=root/'src/sensorless_control.c'
m=root/'src/main.c'
mk=root/'Makefile'

s=c.read_text()
marker='static void current_control(sensorless_control_t *s, int32_t i_alpha, int32_t i_beta, uint16_t phase, int32_t iq_ref, uint16_t pwm_arr) {'
if s.count(marker)!=1:
    raise SystemExit('unexpected current_control anchor')

helpers=r'''/*
 * Q15 PI accumulator: preserve fractional integral contributions at the
 * deliberately tiny first-bench current levels.
 */
static int32_t pi_integrate_q15(int32_t acc_q15, int32_t error) {
    const int64_t limit=(int64_t)PI_INT_LIMIT*32768LL;
    const int64_t next=(int64_t)acc_q15+(int64_t)error*PI_KI_Q15;
    return (int32_t)clamp64(next,-limit,limit);
}

static int32_t pi_duty_from_q15(int32_t error, int32_t acc_q15) {
    const int64_t raw=(int64_t)error*PI_KP_Q15+acc_q15;
    return (int32_t)clamp64(raw/32768LL,-MOD_LIMIT,MOD_LIMIT);
}

'''
s=s.replace(marker,helpers+marker,1)

old='''    s->id_int = clamp32(s->id_int + ((ed * PI_KI_Q15) >> 15), -PI_INT_LIMIT, PI_INT_LIMIT);
    s->iq_int = clamp32(s->iq_int + ((eq * PI_KI_Q15) >> 15), -PI_INT_LIMIT, PI_INT_LIMIT);
    int32_t vd = clamp32(((ed * PI_KP_Q15) >> 15) + s->id_int, -MOD_LIMIT, MOD_LIMIT);
    int32_t vq = clamp32(((eq * PI_KP_Q15) >> 15) + s->iq_int, -MOD_LIMIT, MOD_LIMIT);'''
new='''    s->id_int = pi_integrate_q15(s->id_int, ed);
    s->iq_int = pi_integrate_q15(s->iq_int, eq);
    int32_t vd = pi_duty_from_q15(ed, s->id_int);
    int32_t vq = pi_duty_from_q15(eq, s->iq_int);'''
if s.count(old)!=1:
    raise SystemExit('legacy PI block not found exactly once')
s=s.replace(old,new,1)
c.write_text(s)

s=m.read_text()
if '#define FW_BUILD 0x0720u' not in s:
    raise SystemExit('v0.7.2 build identity missing')
s=s.replace('#define FW_BUILD 0x0720u','#define FW_BUILD 0x0730u',1)
s=s.replace('DeltaESC G30D v0.7.2 MOTOR TEST BENCH','DeltaESC G30D v0.7.3 MOTOR TEST BENCH Q15')
m.write_text(s)

s=mk.read_text()
s=s.replace('v0_7_2','v0_7_3')
mk.write_text(s)

test=root/'tools/pi_q15_bench_test.c'
test.write_text(r'''#include <stdint.h>
#include <stdio.h>
#include "sensorless_control.h"

#define LIMIT_Q15 (2600 * 32768)

static int small_error(void){
    sensorless_control_t s;
    phase_current_counts_t z={0,0,0};
    sensorless_control_init(&s);
    sensorless_control_set_drive(&s,1u);
    int32_t expect=0;
    for(unsigned k=1;k<=400u;k++){
        sensorless_control_step(&s,z,2500u,1999u,1u);
        if(s.state!=SENSORLESS_ALIGN)return 1;
        int32_t iq=(k<5u)?(int32_t)k:5;
        expect += iq*220;
        if(s.iq_int!=expect)return 2;
        if(s.id_int!=0)return 3;
        if(s.ccr1>1999u||s.ccr2>1999u||s.ccr3>1999u)return 4;
    }
    if(s.iq_int!=437800)return 5;
    sensorless_control_stop(&s);
    if(s.id_int||s.iq_int)return 6;
    return 0;
}

static int clamp_and_stress(void){
    sensorless_control_t s;
    sensorless_control_init(&s);
    sensorless_control_set_drive(&s,1u);
    s.iq_int=LIMIT_Q15-1;
    sensorless_control_step(&s,(phase_current_counts_t){0,0,0},2500u,1999u,1u);
    if(s.iq_int!=LIMIT_Q15)return 7;
    for(unsigned k=0;k<10000u;k++){
        int16_t i=(int16_t)(((k*173u)%121u)-60);
        sensorless_control_step(&s,(phase_current_counts_t){i,(int16_t)(i/2),(int16_t)-i},
                                (uint16_t)(1700u+k%300u),1999u,1u);
        if(s.ccr1>1999u||s.ccr2>1999u||s.ccr3>1999u)return 8;
        if(s.iq_int<-LIMIT_Q15||s.iq_int>LIMIT_Q15)return 9;
        if(s.id_int<-LIMIT_Q15||s.id_int>LIMIT_Q15)return 10;
        if(s.state==SENSORLESS_FAULT){
            sensorless_control_stop(&s);
            sensorless_control_set_drive(&s,1u);
        }
    }
    return 0;
}

int main(void){
    int r=small_error(); if(r)return r;
    r=clamp_and_stress(); if(r)return r;
    puts("PASS v0.7.3 Q15 small-error integration and PWM bounds");
    return 0;
}
''')
print('v0.7.3 Q15 bench overlay applied')
