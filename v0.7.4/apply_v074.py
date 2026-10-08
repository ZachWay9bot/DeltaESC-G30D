#!/usr/bin/env python3
from pathlib import Path

root=Path(__file__).resolve().parent
h=root/'src/sensorless_control.h'
c=root/'src/sensorless_control.c'
m=root/'src/main.c'
mk=root/'Makefile'

s=h.read_text()
old='''    int16_t observer_speed_ehz_q8;
    uint16_t speed_lock_ticks;
'''
new='''    int32_t observer_speed_ehz_q8;
    int32_t observer_speed_filt_ehz_q8;
    uint16_t speed_lock_ticks;
'''
if s.count(old)!=1: raise SystemExit('speed fields anchor mismatch')
s=s.replace(old,new,1)
anchor='uint8_t sensorless_control_set_motor_params(sensorless_control_t *s, uint32_t r_uohm, uint32_t l_nh, uint32_t flux_uwb);\n'
if s.count(anchor)!=1: raise SystemExit('header function anchor mismatch')
s=s.replace(anchor,anchor+'int32_t sensorless_control_phase_delta_to_ehz_q8(int16_t phase_delta);\n',1)
h.write_text(s)

s=c.read_text()
s=s.replace('#define SPEED_LOCK_REQUIRED_TICKS 160u',
'''#define SPEED_LOCK_REQUIRED_TICKS 160u
#define MAX_SPEED_DISAGREE_EHZ_Q8 (20 * 256)''',1)

insert='''int32_t sensorless_control_phase_delta_to_ehz_q8(int16_t phase_delta) {
    return ((int32_t)phase_delta * (int32_t)CONTROL_HZ) / 256;
}

'''
anchor='static int16_t ma_to_counts(uint16_t ma) {'
if s.count(anchor)!=1: raise SystemExit('ma_to_counts anchor mismatch')
s=s.replace(anchor,insert+anchor,1)

old='''        int16_t d = phase_delta(s->observer_phase, old_phase);
        int32_t ehz_q8 = ((int32_t)d * (int32_t)CONTROL_HZ) >> 8;
        s->observer_speed_ehz_q8 = (int16_t)clamp32(ehz_q8, -32768, 32767);
'''
new='''        int16_t d = phase_delta(s->observer_phase, old_phase);
        s->observer_speed_ehz_q8 = sensorless_control_phase_delta_to_ehz_q8(d);
        s->observer_speed_filt_ehz_q8 +=
            (s->observer_speed_ehz_q8 - s->observer_speed_filt_ehz_q8) / 8;
'''
if s.count(old)!=1: raise SystemExit('observer speed block mismatch')
s=s.replace(old,new,1)

s=s.replace('s->observer_speed_ehz_q8 = 0; s->speed_lock_ticks = 0u;',
            's->observer_speed_ehz_q8 = 0; s->observer_speed_filt_ehz_q8 = 0; s->speed_lock_ticks = 0u;',1)

# observer_reset already clears state variables; explicitly clear both speeds anywhere it resets handover state.
old='''        int32_t abs_speed = s->observer_speed_ehz_q8; if (abs_speed < 0) abs_speed = -abs_speed;
        if (s->observer_valid && pe < PHASE_LOCK_LIMIT_U16 &&
            abs_speed >= MIN_HANDOVER_EHZ_Q8 && abs_speed <= MAX_HANDOVER_EHZ_Q8 &&
            s->state_ticks > OPEN_LOOP_MIN_TICKS) {
            if (s->lock_ticks < 0xFFFFu) s->lock_ticks++;
            if (s->speed_lock_ticks < 0xFFFFu) s->speed_lock_ticks++;
        } else { s->lock_ticks = 0; s->speed_lock_ticks = 0; }
'''
new='''        int32_t speed = s->observer_speed_filt_ehz_q8;
        int32_t disagreement = s->observer_speed_ehz_q8 - speed;
        if (disagreement < 0) disagreement = -disagreement;
        if (s->observer_valid && pe < PHASE_LOCK_LIMIT_U16 &&
            speed >= MIN_HANDOVER_EHZ_Q8 && speed <= MAX_HANDOVER_EHZ_Q8 &&
            disagreement <= MAX_SPEED_DISAGREE_EHZ_Q8 &&
            s->state_ticks > OPEN_LOOP_MIN_TICKS) {
            if (s->lock_ticks < 0xFFFFu) s->lock_ticks++;
            if (s->speed_lock_ticks < 0xFFFFu) s->speed_lock_ticks++;
        } else { s->lock_ticks = 0; s->speed_lock_ticks = 0; }
'''
if s.count(old)!=1: raise SystemExit('handover speed block mismatch')
s=s.replace(old,new,1)
c.write_text(s)

s=m.read_text()
if '#define FW_BUILD 0x0730u' not in s: raise SystemExit('v0.7.3 identity missing')
s=s.replace('#define FW_BUILD 0x0730u','#define FW_BUILD 0x0740u',1)
old='''case 0xD7u:{uint16_t conf=0u;if(ctrl.observer_valid){int32_t e=ctrl.phase_error;if(e<0)e=-e;if(e>5461)e=5461;conf=(uint16_t)(1000-(e*1000)/5461);}p[0]=ctrl.state;p[1]=ctrl.observer_valid;put_u16(p+2,ctrl.lock_ticks);put_u16(p+4,ctrl.lost_ticks);put_u16(p+6,conf);put_i16(p+8,ctrl.observer_speed_ehz_q8);n=10u;break;}'''
new='''case 0xD7u:{uint16_t conf=0u;if(ctrl.observer_valid){int32_t e=ctrl.phase_error;if(e<0)e=-e;if(e>5461)e=5461;conf=(uint16_t)(1000-(e*1000)/5461);}p[0]=ctrl.state;p[1]=ctrl.observer_valid;put_u16(p+2,ctrl.lock_ticks);put_u16(p+4,ctrl.lost_ticks);put_u16(p+6,conf);put_i16(p+8,(int16_t)(ctrl.observer_speed_ehz_q8/256));put_i16(p+10,(int16_t)(ctrl.observer_speed_filt_ehz_q8/256));n=12u;break;}'''
if s.count(old)!=1: raise SystemExit('D7 anchor mismatch')
s=s.replace(old,new,1)
s=s.replace('DeltaESC G30D v0.7.3 MOTOR TEST BENCH Q15','DeltaESC G30D v0.7.4 OBSERVER HANDOVER',1)
m.write_text(s)

s=mk.read_text().replace('v0_7_3','v0_7_4')
mk.write_text(s)

test=root/'tools/observer_speed_host_test.c'
test.write_text(r'''#include <stdint.h>
#include <stdio.h>
#include "sensorless_control.h"

int main(void){
    if(sensorless_control_phase_delta_to_ehz_q8(0)!=0)return 1;
    if(sensorless_control_phase_delta_to_ehz_q8(16384)!=(1000*256))return 2;
    if(sensorless_control_phase_delta_to_ehz_q8(-16384)!=-(1000*256))return 3;
    if(sensorless_control_phase_delta_to_ehz_q8(32767)<=32767)return 4;

    sensorless_control_t s;
    sensorless_control_init(&s);
    if(s.observer_speed_ehz_q8!=0 || s.observer_speed_filt_ehz_q8!=0)return 5;

    puts("PASS v0.7.4 observer speed range exceeds int16 Q8 and initializes cleanly");
    return 0;
}
''')
print('v0.7.4 observer/handover overlay applied')
