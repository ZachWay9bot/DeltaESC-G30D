#!/usr/bin/env python3
"""Convert v0.7.0 FOC PI integrators to fractional Q15 on a gate-OFF probe."""
from pathlib import Path
import sys
root=Path(sys.argv[1])
fn=root/'src/sensorless_control.c'
s=fn.read_text()
marker='static void current_control(sensorless_control_t *s, int32_t i_alpha, int32_t i_beta, uint16_t phase, int32_t iq_ref, uint16_t pwm_arr) {'
helpers='''/*
 * A fractional Q15 PI integrator preserves small errors. The previous
 * (err * Ki)>>15 rounding before accumulation discarded every small sample.
 * Kp/Ki are STILL provisional; physical calibration is not demonstrated.
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
assert s.count(marker)==1
s=s.replace(marker,helpers+marker,1)
old='''    s->id_int = clamp32(s->id_int + ((ed * PI_KI_Q15) >> 15), -PI_INT_LIMIT, PI_INT_LIMIT);
    s->iq_int = clamp32(s->iq_int + ((eq * PI_KI_Q15) >> 15), -PI_INT_LIMIT, PI_INT_LIMIT);
    int32_t vd = clamp32(((ed * PI_KP_Q15) >> 15) + s->id_int, -MOD_LIMIT, MOD_LIMIT);
    int32_t vq = clamp32(((eq * PI_KP_Q15) >> 15) + s->iq_int, -MOD_LIMIT, MOD_LIMIT);'''
new='''    s->id_int = pi_integrate_q15(s->id_int, ed);
    s->iq_int = pi_integrate_q15(s->iq_int, eq);
    int32_t vd = pi_duty_from_q15(ed, s->id_int);
    int32_t vq = pi_duty_from_q15(eq, s->iq_int);'''
assert s.count(old)==1
fn.write_text(s.replace(old,new,1))
main=root/'src/main.c'
s=main.read_text()
a='#define FW_BUILD 0x0700u'
assert s.count(a)==1
s=s.replace(a,'#define FW_BUILD 0x0701u',1)
a='v0.7.0 motor probe must NEVER arm a physical power stage'
assert s.count(a)==1
s=s.replace(a,'v0.7.1 FOC PI math audit must NEVER arm a physical power stage',1)
a='DeltaESC G30D v0.7.0 ST-LINK PASSIVE MOTOR ADC PROBE.'
assert s.count(a)==1
s=s.replace(a,'DeltaESC G30D v0.7.1 PASSIVE MOTOR PI MATH AUDIT.',1)
main.write_text(s)
make=root/'Makefile'
s=make.read_text()
assert 'all: safe\n' in s and 'active: ' not in s and 'bench: ' not in s
assert s.count('v0_7_0')==8
make.write_text(s.replace('v0_7_0','v0_7_1'))
print('v0.7.1: fractional Q15 PI; gates remain compile-disabled')
