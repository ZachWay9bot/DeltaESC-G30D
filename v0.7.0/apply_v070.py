#!/usr/bin/env python3
from pathlib import Path

root=Path(__file__).resolve().parent
h=root/'src/sensorless_control.h'
c=root/'src/sensorless_control.c'
m=root/'src/main.c'
mk=root/'Makefile'

s=h.read_text()
s=s.replace('    uint16_t run_current_ma;\n    int16_t run_iq_counts;\n',
'''    uint16_t run_current_ma;
    int16_t run_iq_counts;
    int16_t iq_ref_slewed;

    uint32_t motor_r_uohm;
    uint32_t motor_l_nh;
    uint32_t flux_uwb;
    int16_t observer_speed_ehz_q8;
    uint16_t speed_lock_ticks;
''')
s=s.replace('uint8_t sensorless_control_set_run_current_ma(sensorless_control_t *s, uint16_t current_ma);\n',
'''uint8_t sensorless_control_set_run_current_ma(sensorless_control_t *s, uint16_t current_ma);
uint8_t sensorless_control_set_motor_params(sensorless_control_t *s, uint32_t r_uohm, uint32_t l_nh, uint32_t flux_uwb);
''')
h.write_text(s)

s=c.read_text()
s=s.replace('#define DEFAULT_RUN_CURRENT_MA 500u\n',
'''#define DEFAULT_RUN_CURRENT_MA 500u
#define IQ_SLEW_COUNTS_PER_TICK 1
#define MIN_HANDOVER_EHZ_Q8 (8 * 256)
#define MAX_HANDOVER_EHZ_Q8 (220 * 256)
#define SPEED_LOCK_REQUIRED_TICKS 160u
''')
s=s.replace(
'''    const int64_t l = (3LL * SENSORLESS_MOTOR_L_PARAM) >> 1;
    const int64_t r = (3LL * SENSORLESS_MOTOR_R_PARAM) >> 1;
    const int64_t lambda = SENSORLESS_FLUX_LINKAGE;
''',
'''    const int64_t l = (int64_t)((3u * s->motor_l_nh) / 2000u);
    const int64_t r = (int64_t)((3u * s->motor_r_uohm) / 2000u);
    const int64_t lambda = (int64_t)s->flux_uwb;
''')
s=s.replace(
'''    s->observer_phase = (uint16_t)(fast_atan2_u16(-s->e_beta, s->e_alpha) + s->phase_offset_u16);
}
''',
'''    {
        uint16_t old_phase = s->observer_phase;
        s->observer_phase = (uint16_t)(fast_atan2_u16(-s->e_beta, s->e_alpha) + s->phase_offset_u16);
        int16_t d = phase_delta(s->observer_phase, old_phase);
        int32_t ehz_q8 = ((int32_t)d * (int32_t)CONTROL_HZ) >> 8;
        s->observer_speed_ehz_q8 = (int16_t)clamp32(ehz_q8, -32768, 32767);
    }
}
''')
s=s.replace(
'''    s->phase_offset_u16 = 0u; s->run_current_ma = DEFAULT_RUN_CURRENT_MA; s->run_iq_counts = ma_to_counts(DEFAULT_RUN_CURRENT_MA);
    observer_reset(s); s->ccr1 = s->ccr2 = s->ccr3 = 0;
''',
'''    s->phase_offset_u16 = 0u; s->run_current_ma = DEFAULT_RUN_CURRENT_MA; s->run_iq_counts = ma_to_counts(DEFAULT_RUN_CURRENT_MA);
    s->iq_ref_slewed = 0;
    s->motor_r_uohm = 90000u; s->motor_l_nh = 100000u; s->flux_uwb = 1800u;
    s->observer_speed_ehz_q8 = 0; s->speed_lock_ticks = 0u;
    observer_reset(s); s->ccr1 = s->ccr2 = s->ccr3 = 0;
''')
needle='''uint8_t sensorless_control_set_run_current_ma(sensorless_control_t *s, uint16_t current_ma) {
    if (current_ma < 100u || current_ma > 2000u) return 0u;
    s->run_current_ma = current_ma;
    s->run_iq_counts = ma_to_counts(current_ma);
    return 1u;
}
'''
s=s.replace(needle, needle+'''
uint8_t sensorless_control_set_motor_params(sensorless_control_t *s, uint32_t r_uohm, uint32_t l_nh, uint32_t flux_uwb) {
    if (!s) return 0u;
    if (r_uohm < 1000u || r_uohm > 2000000u) return 0u;
    if (l_nh < 1000u || l_nh > 5000000u) return 0u;
    if (flux_uwb < 100u || flux_uwb > 1000000u) return 0u;
    s->motor_r_uohm = r_uohm;
    s->motor_l_nh = l_nh;
    s->flux_uwb = flux_uwb;
    observer_reset(s);
    return 1u;
}
''')
s=s.replace('s->id_int = 0; s->iq_int = 0; observer_reset(s);',
            's->id_int = 0; s->iq_int = 0; s->iq_ref_slewed = 0; s->speed_lock_ticks = 0u; observer_reset(s);',1)
s=s.replace('s->lock_ticks = 0; s->lost_ticks = 0; s->id_int = 0; s->iq_int = 0;',
            's->lock_ticks = 0; s->lost_ticks = 0; s->speed_lock_ticks = 0u; s->id_int = 0; s->iq_int = 0; s->iq_ref_slewed = 0;')
s=s.replace(
'''        if (s->observer_valid && pe < PHASE_LOCK_LIMIT_U16 && s->state_ticks > OPEN_LOOP_MIN_TICKS) {
            if (s->lock_ticks < 0xFFFFu) s->lock_ticks++;
        } else s->lock_ticks = 0;
        if (s->lock_ticks >= LOCK_REQUIRED_TICKS) { s->state = SENSORLESS_HANDOVER; s->state_ticks = 0; }
''',
'''        int32_t abs_speed = s->observer_speed_ehz_q8; if (abs_speed < 0) abs_speed = -abs_speed;
        if (s->observer_valid && pe < PHASE_LOCK_LIMIT_U16 &&
            abs_speed >= MIN_HANDOVER_EHZ_Q8 && abs_speed <= MAX_HANDOVER_EHZ_Q8 &&
            s->state_ticks > OPEN_LOOP_MIN_TICKS) {
            if (s->lock_ticks < 0xFFFFu) s->lock_ticks++;
            if (s->speed_lock_ticks < 0xFFFFu) s->speed_lock_ticks++;
        } else { s->lock_ticks = 0; s->speed_lock_ticks = 0; }
        if (s->lock_ticks >= LOCK_REQUIRED_TICKS && s->speed_lock_ticks >= SPEED_LOCK_REQUIRED_TICKS) {
            s->state = SENSORLESS_HANDOVER; s->state_ticks = 0;
        }
''')
s=s.replace(
'''    int32_t iq_ref = (s->state == SENSORLESS_ALIGN) ? ALIGN_IQ_COUNTS : s->run_iq_counts;
    current_control(s, i_alpha, i_beta, s->control_phase, iq_ref, pwm_arr);
''',
'''    int32_t iq_target = (s->state == SENSORLESS_ALIGN) ? ALIGN_IQ_COUNTS : s->run_iq_counts;
    if (s->iq_ref_slewed < iq_target) {
        s->iq_ref_slewed += IQ_SLEW_COUNTS_PER_TICK;
        if (s->iq_ref_slewed > iq_target) s->iq_ref_slewed = (int16_t)iq_target;
    } else if (s->iq_ref_slewed > iq_target) {
        s->iq_ref_slewed -= IQ_SLEW_COUNTS_PER_TICK;
        if (s->iq_ref_slewed < iq_target) s->iq_ref_slewed = (int16_t)iq_target;
    }
    current_control(s, i_alpha, i_beta, s->control_phase, s->iq_ref_slewed, pwm_arr);
''')
c.write_text(s)

s=m.read_text()
s=s.replace('#define FW_BUILD 0x0605u','#define FW_BUILD 0x0700u')
s=s.replace(
"case 0xD7u:{uint16_t conf=0u;if(ctrl.observer_valid){int32_t e=ctrl.phase_error;if(e<0)e=-e;if(e>5461)e=5461;conf=(uint16_t)(1000-(e*1000)/5461);}p[0]=ctrl.state;p[1]=ctrl.observer_valid;put_u16(p+2,ctrl.lock_ticks);put_u16(p+4,ctrl.lost_ticks);put_u16(p+6,conf);n=8u;break;}",
"case 0xD7u:{uint16_t conf=0u;if(ctrl.observer_valid){int32_t e=ctrl.phase_error;if(e<0)e=-e;if(e>5461)e=5461;conf=(uint16_t)(1000-(e*1000)/5461);}p[0]=ctrl.state;p[1]=ctrl.observer_valid;put_u16(p+2,ctrl.lock_ticks);put_u16(p+4,ctrl.lost_ticks);put_u16(p+6,conf);put_i16(p+8,ctrl.observer_speed_ehz_q8);n=10u;break;}")
for reg,var in [('F0','g_cfg_r_uohm'),('F1','g_cfg_l_nh'),('F2','g_cfg_flux_uwb')]:
    pass
s=s.replace('g_cfg_r_uohm=v;g_cfg_dirty=1u;action_ack(f,cmd,ACT_OK);return;',
            'g_cfg_r_uohm=v;if(!sensorless_control_set_motor_params(&ctrl,g_cfg_r_uohm,g_cfg_l_nh,g_cfg_flux_uwb)){action_ack(f,cmd,ACT_BAD_RANGE);return;}g_cfg_dirty=1u;action_ack(f,cmd,ACT_OK);return;')
s=s.replace('g_cfg_l_nh=v;g_cfg_dirty=1u;action_ack(f,cmd,ACT_OK);return;',
            'g_cfg_l_nh=v;if(!sensorless_control_set_motor_params(&ctrl,g_cfg_r_uohm,g_cfg_l_nh,g_cfg_flux_uwb)){action_ack(f,cmd,ACT_BAD_RANGE);return;}g_cfg_dirty=1u;action_ack(f,cmd,ACT_OK);return;')
s=s.replace('g_cfg_flux_uwb=v;g_cfg_dirty=1u;action_ack(f,cmd,ACT_OK);return;',
            'g_cfg_flux_uwb=v;if(!sensorless_control_set_motor_params(&ctrl,g_cfg_r_uohm,g_cfg_l_nh,g_cfg_flux_uwb)){action_ack(f,cmd,ACT_BAD_RANGE);return;}g_cfg_dirty=1u;action_ack(f,cmd,ACT_OK);return;')
s=s.replace('sensorless_control_init(&ctrl);\n    adc1_injected_init()',
            'sensorless_control_init(&ctrl);sensorless_control_set_motor_params(&ctrl,g_cfg_r_uohm,g_cfg_l_nh,g_cfg_flux_uwb);\n    adc1_injected_init()')
s=s.replace('DeltaESC G30D v0.6.5 G30-framing + staged-IAP firmware','DeltaESC G30D v0.7 motor-core development')
m.write_text(s)

s=mk.read_text().replace('v0_6_5','v0_7_0')
mk.write_text(s)

test=root/'tools/motor_core_host_test.c'
test.write_text(r'''#include <assert.h>
#include <stdint.h>
#include "sensorless_control.h"
int main(void) {
    sensorless_control_t s;
    sensorless_control_init(&s);
    assert(s.state == SENSORLESS_STOP);
    assert(s.run_current_ma == 500u);
    assert(s.iq_ref_slewed == 0);
    assert(sensorless_control_set_motor_params(&s,90000u,100000u,1800u)==1u);
    assert(s.motor_r_uohm==90000u && s.motor_l_nh==100000u && s.flux_uwb==1800u);
    assert(sensorless_control_set_motor_params(&s,999u,100000u,1800u)==0u);
    assert(sensorless_control_set_motor_params(&s,90000u,999u,1800u)==0u);
    assert(sensorless_control_set_motor_params(&s,90000u,100000u,99u)==0u);
    assert(sensorless_control_set_run_current_ma(&s,100u)==1u);
    assert(sensorless_control_set_run_current_ma(&s,2000u)==1u);
    assert(sensorless_control_set_run_current_ma(&s,99u)==0u);
    assert(sensorless_control_set_run_current_ma(&s,2001u)==0u);
    sensorless_control_set_drive(&s,1u);
    assert(s.drive_request==1u && s.state==SENSORLESS_ALIGN && s.iq_ref_slewed==0);
    sensorless_control_stop(&s);
    assert(s.drive_request==0u && s.state==SENSORLESS_STOP && s.iq_ref_slewed==0);
    return 0;
}
''')
print('v0.7.0 transform applied')
