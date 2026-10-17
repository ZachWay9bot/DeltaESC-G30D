#!/usr/bin/env python3
"""DeltaESC v0.10.0: import the missing XBOT FO69/FO70 sensorless pieces.

Source/link-only overlay. It imports:
- FO70 passive PA6/PA7/PB1 BEMF rolling detector and kick-speed gate,
- FO70 passive regular-ADC scheduling while the bridge is disarmed,
- FO69 observer confidence / phase-step / quiet-clamp takeover criteria,
- rolling-start entry that skips ALIGN/OPEN and verifies at zero Iq,
- neutral->kick interlock and a modest post-lock Iq ramp,
- read-only FC rolling telemetry.

It does NOT copy XBOT current gain, current trips, deadtime, phase enable polarity,
or produce a flashable motor image.
"""
from pathlib import Path
import shutil,sys,re

root=Path(__file__).resolve().parents[1]
target=Path(sys.argv[1]).resolve()
main=target/'src/main.c'; make=target/'Makefile'
sc=target/'src/sensorless_control.c'; sh=target/'src/sensorless_control.h'
fluxc=target/'src/vesc_ebics_flux.c'; fluxh=target/'src/vesc_ebics_flux.h'
mpc=target/'src/motor_pipeline.c'; mph=target/'src/motor_pipeline.h'

def one(s,old,new,label):
    n=s.count(old)
    if n!=1:
        raise RuntimeError(f'v0.10.0 {label}: expected one anchor, got {n}: {old[:100]!r}')
    return s.replace(old,new,1)

def copy_new(name):
    src=root/'src'/name; dst=target/'src'/name
    if dst.exists(): raise RuntimeError(f'v0.10.0 source already exists: {name}')
    shutil.copyfile(src,dst)

for f in ['g30_rolling_bemf.c','g30_rolling_bemf.h',
          'g30_passive_bemf_adc.c','g30_passive_bemf_adc.h']:
    copy_new(f)

# FO69 quiet takeover needs observer clamp evidence.
s=fluxh.read_text()
s=one(s,'    uint16_t samples;\n    uint8_t valid;',
'''    uint16_t samples;
    uint16_t clamp_events; /* FO69 quiet-takeover evidence; saturating counter */
    uint8_t valid;''','flux clamp field')
fluxh.write_text(s)

s=fluxc.read_text()
s=one(s,
'''    s->x_alpha_nwb = bounded((int64_t)s->x_alpha_nwb + delta_a + correction_a, limit);
    s->x_beta_nwb = bounded((int64_t)s->x_beta_nwb + delta_b + correction_b, limit);''',
'''    const int64_t next_a=(int64_t)s->x_alpha_nwb + delta_a + correction_a;
    const int64_t next_b=(int64_t)s->x_beta_nwb + delta_b + correction_b;
    if((next_a>limit || next_a<-(int64_t)limit ||
        next_b>limit || next_b<-(int64_t)limit) && s->clamp_events<65535u)
        s->clamp_events++;
    s->x_alpha_nwb = bounded(next_a, limit);
    s->x_beta_nwb = bounded(next_b, limit);''','flux clamp counter')
fluxc.write_text(s)

# Add ROLL_VERIFY without changing existing numeric state IDs.
s=sh.read_text()
s=one(s,
'''    SENSORLESS_CLOSED_LOOP = 4,
    SENSORLESS_FAULT = 5
} sensorless_state_id_t;''',
'''    SENSORLESS_CLOSED_LOOP = 4,
    SENSORLESS_FAULT = 5,
    SENSORLESS_ROLL_VERIFY = 6
} sensorless_state_id_t;''','rolling enum')
s=one(s,'    uint16_t openloop_step;\n',
'''    uint16_t openloop_step;
    int16_t rolling_step;
    uint16_t rolling_good_ticks;
    uint16_t rolling_iq_ticks;
    uint16_t rolling_clamp_start;
    uint32_t rolling_phase_error_sum;
    uint8_t rolling_start;
''','rolling state fields')
s=one(s,
'void sensorless_control_set_drive(sensorless_control_t *s, uint8_t enable);\n',
'''void sensorless_control_set_drive(sensorless_control_t *s, uint8_t enable);
/* FO70 kick-start: seed the observer from passive BEMF and enter zero-Iq
 * verification. Physical bridge arming remains owned by motor_pipeline. */
uint8_t sensorless_control_start_rolling(sensorless_control_t *s,
                                         uint16_t phase_u16,
                                         int16_t electrical_step_u16);
''','rolling API')
sh.write_text(s)

s=sc.read_text()
s=one(s,'#define OBS_LOST_TICKS            160u   /* 40 ms */\n',
'''#define OBS_LOST_TICKS            160u   /* 40 ms */
/* FO69 used 80 confidence evaluations at 1 kHz. DeltaESC evaluates at 4 kHz,
 * so preserve the ~80 ms quiet-verification time rather than merely 80 ticks. */
#define ROLL_VERIFY_TICKS           320u
#define ROLL_VERIFY_TIMEOUT_TICKS  1200u
#define ROLL_IQ_RAMP_TICKS         1000u
#define ROLL_MAX_IQ_COUNTS           12
#define ROLL_PHASE_NOW_MAX_U16     8192u
#define ROLL_PHASE_AVG_MAX_U16     5461u
#define ROLL_PHASE_OVER90_U16     16384u
#define ROLL_STEP_ABS_MAX_U16       1024u
#define ROLL_STEP_POS_MAX_U16        256u
''','rolling constants')

anchor='static void vector_to_pwm(sensorless_control_t *s, int32_t v_alpha, int32_t v_beta, uint16_t pwm_arr) {'
helper=r'''static uint32_t rolling_flux_magnitude_uwb(const sensorless_control_t *s) {
    uint32_t a=abs32u(s->vesc_flux.flux_alpha_nwb);
    uint32_t b=abs32u(s->vesc_flux.flux_beta_nwb);
    uint32_t mx=a>b?a:b,mn=a>b?b:a;
    return (mx+(mn>>1))/1000u;
}
static uint8_t rolling_phase_step_ok(int16_t step) {
    uint16_t a=(uint16_t)(step<0?-(int32_t)step:(int32_t)step);
    return (uint8_t)(a<=ROLL_STEP_ABS_MAX_U16 && step<=(int16_t)ROLL_STEP_POS_MAX_U16);
}
static uint8_t rolling_flux_instant_ok(const sensorless_control_t *s) {
    if(!s->motor_params_valid || !s->vesc_flux.valid || !s->motor_flux_uwb)return 0u;
    uint32_t mag=rolling_flux_magnitude_uwb(s);
    if(mag<s->motor_flux_uwb/2u || mag>((s->motor_flux_uwb*3u)/2u))return 0u;
    if(!rolling_phase_step_ok(s->observer_step))return 0u;
    return 1u;
}

'''
s=one(s,anchor,helper+anchor,'rolling observer helpers')

old_init='''void sensorless_control_init(sensorless_control_t *s) {
    s->drive_request = 0; s->state = SENSORLESS_STOP; s->fault = 0; s->state_ticks = 0;
    s->openloop_step = OPENLOOP_STEP_START; s->openloop_phase = 0; s->control_phase = 0;
'''
new_init='''void sensorless_control_init(sensorless_control_t *s) {
    s->drive_request = 0; s->state = SENSORLESS_STOP; s->fault = 0; s->state_ticks = 0;
    s->openloop_step = OPENLOOP_STEP_START; s->openloop_phase = 0; s->control_phase = 0;
    s->rolling_step=0;s->rolling_good_ticks=0u;s->rolling_iq_ticks=0u;
    s->rolling_clamp_start=0u;s->rolling_phase_error_sum=0u;s->rolling_start=0u;
'''
s=one(s,old_init,new_init,'rolling init')

anchor='void sensorless_control_set_drive(sensorless_control_t *s, uint8_t enable) {'
rolling_start=r'''uint8_t sensorless_control_start_rolling(sensorless_control_t *s,
                                         uint16_t phase_u16,
                                         int16_t electrical_step_u16) {
    if(!s || s->fault || s->state==SENSORLESS_FAULT || !s->motor_params_valid ||
       !s->motor_flux_uwb || electrical_step_u16>=0 ||
       !rolling_phase_step_ok(electrical_step_u16)) return 0u;
    s->drive_request=1u;s->state=SENSORLESS_ROLL_VERIFY;s->state_ticks=0u;
    s->openloop_phase=phase_u16;s->control_phase=phase_u16;
    s->rolling_step=electrical_step_u16;s->rolling_good_ticks=0u;
    s->rolling_iq_ticks=0u;s->rolling_phase_error_sum=0u;s->rolling_start=1u;
    s->id_int=0;s->iq_int=0;observer_reset(s);
    const int32_t lambda=(int32_t)(s->motor_flux_uwb*1000u);
    const int32_t fa=(int32_t)(((int64_t)lambda*cos_q15(phase_u16))>>15);
    const int32_t fb=(int32_t)(((int64_t)lambda*sin_q15(phase_u16))>>15);
    s->vesc_flux.x_alpha_nwb=fa;s->vesc_flux.x_beta_nwb=fb;
    s->vesc_flux.flux_alpha_nwb=fa;s->vesc_flux.flux_beta_nwb=fb;
    s->vesc_flux.samples=2u;s->vesc_flux.valid=1u;
    s->observer_phase=s->observer_phase_prev=phase_u16;
    s->observer_step=electrical_step_u16;
    s->rolling_clamp_start=s->vesc_flux.clamp_events;
    return 1u;
}

'''
s=one(s,anchor,rolling_start+anchor,'rolling start function')
s=one(s,'    if (!s->drive_request) {\n        s->drive_request = 1;',
'''    if (!s->drive_request) {
        s->rolling_start=0u;
        s->drive_request = 1;''','legacy start clears rolling')
s=one(s,
'    s->lock_ticks = 0; s->lost_ticks = 0; s->id_int = 0; s->iq_int = 0;\n',
'''    s->lock_ticks = 0; s->lost_ticks = 0; s->id_int = 0; s->iq_int = 0;
    s->rolling_good_ticks=0u;s->rolling_iq_ticks=0u;s->rolling_phase_error_sum=0u;
    s->rolling_start=0u;s->rolling_step=0;
''','rolling stop')

anchor='''    case SENSORLESS_FAULT:
    default:
        s->drive_request = 0; zero_vector(s, pwm_arr); return;
'''
roll_case=r'''    case SENSORLESS_ROLL_VERIFY: {
#if SENSORLESS_CLOSED_LOOP_ALLOWED
        s->openloop_phase=(uint16_t)(s->openloop_phase+s->rolling_step);
        s->control_phase=s->openloop_phase;
        const int16_t pe_s=phase_delta(s->observer_phase,s->openloop_phase);
        const uint16_t pe=(uint16_t)(pe_s<0?-(int32_t)pe_s:(int32_t)pe_s);
        const uint16_t clamp_delta=(uint16_t)(s->vesc_flux.clamp_events-s->rolling_clamp_start);
        if(pe>=ROLL_PHASE_OVER90_U16 || clamp_delta || !rolling_phase_step_ok(s->observer_step)){
            s->fault=6u;s->state=SENSORLESS_FAULT;break;
        }
        if(rolling_flux_instant_ok(s) && pe<ROLL_PHASE_NOW_MAX_U16){
            if(s->rolling_good_ticks<65535u)s->rolling_good_ticks++;
            s->rolling_phase_error_sum+=pe;
        }else{
            s->rolling_good_ticks=0u;s->rolling_phase_error_sum=0u;
        }
        if(s->rolling_good_ticks>=ROLL_VERIFY_TICKS){
            const uint32_t avg=s->rolling_phase_error_sum/s->rolling_good_ticks;
            if(avg>=ROLL_PHASE_AVG_MAX_U16){s->fault=7u;s->state=SENSORLESS_FAULT;break;}
            s->state=SENSORLESS_CLOSED_LOOP;s->state_ticks=0u;s->lost_ticks=0u;
            s->rolling_iq_ticks=0u;
        }else if(s->state_ticks>=ROLL_VERIFY_TIMEOUT_TICKS){
            s->fault=8u;s->state=SENSORLESS_FAULT;
        }
#else
        s->fault=9u;s->state=SENSORLESS_FAULT;
#endif
        break;
    }
'''
s=one(s,anchor,roll_case+anchor,'rolling verify state')

old='''        s->control_phase = s->observer_phase;
        if (!s->observer_valid) { if (s->lost_ticks < 0xFFFFu) s->lost_ticks++; } else s->lost_ticks = 0;
        if (s->lost_ticks >= OBS_LOST_TICKS) { s->fault = 3; s->state = SENSORLESS_FAULT; }
'''
new='''        s->control_phase = s->observer_phase;
        const uint8_t obs_ok=s->rolling_start?rolling_flux_instant_ok(s):s->observer_valid;
        if (!obs_ok) { if (s->lost_ticks < 0xFFFFu) s->lost_ticks++; } else s->lost_ticks = 0;
        if (s->lost_ticks >= OBS_LOST_TICKS) { s->fault = 3; s->state = SENSORLESS_FAULT; }
'''
s=one(s,old,new,'rolling closed-loop observer validity')

old='''    int32_t id_ref = (s->state == SENSORLESS_ALIGN) ? s->run_iq_counts : 0;
    int32_t iq_ref = (s->state == SENSORLESS_ALIGN) ? 0 : s->run_iq_counts;
    current_control(s, i_alpha, i_beta, s->control_phase, id_ref, iq_ref, vbus_adc, pwm_arr);
'''
new='''    int32_t id_ref = (s->state == SENSORLESS_ALIGN) ? s->run_iq_counts : 0;
    int32_t iq_ref = (s->state == SENSORLESS_ALIGN) ? 0 : s->run_iq_counts;
    if(s->rolling_start){
        id_ref=0;
        if(s->state==SENSORLESS_ROLL_VERIFY)iq_ref=0;
        else if(s->state==SENSORLESS_CLOSED_LOOP){
            int32_t target=s->run_iq_counts;
            if(target>ROLL_MAX_IQ_COUNTS)target=ROLL_MAX_IQ_COUNTS;
            if(s->rolling_iq_ticks<ROLL_IQ_RAMP_TICKS)s->rolling_iq_ticks++;
            iq_ref=(int32_t)(((int64_t)target*s->rolling_iq_ticks)/ROLL_IQ_RAMP_TICKS);
        }else iq_ref=0;
    }
    current_control(s, i_alpha, i_beta, s->control_phase, id_ref, iq_ref, vbus_adc, pwm_arr);
'''
s=one(s,old,new,'rolling iq ramp')
s=one(s,'case SENSORLESS_CLOSED_LOOP: return "CLOSED"; case SENSORLESS_FAULT: return "FAULT";',
      'case SENSORLESS_CLOSED_LOOP: return "CLOSED"; case SENSORLESS_FAULT: return "FAULT"; case SENSORLESS_ROLL_VERIFY: return "ROLL_VERIFY";',
      'rolling state name')
sc.write_text(s)

# Add neutral-gated rolling entry to the existing ADC->FOC->TIM1 pipeline.
s=mph.read_text()
s=one(s,'    uint8_t fault_latched;\n','    uint8_t fault_latched;\n    uint8_t neutral_seen;\n','pipeline neutral field')
s=one(s,
'''uint8_t motor_pipeline_start(motor_pipeline_t *p, const uint16_t offsets[3],
                             uint8_t gate_qualified, uint8_t adc_qualified,
                             uint8_t throttle, uint8_t brake,
                             uint8_t dashboard_valid, uint32_t now_ms);
''',
'''uint8_t motor_pipeline_start(motor_pipeline_t *p, const uint16_t offsets[3],
                             uint8_t gate_qualified, uint8_t adc_qualified,
                             uint8_t throttle, uint8_t brake,
                             uint8_t dashboard_valid, uint32_t now_ms);
uint8_t motor_pipeline_start_rolling(motor_pipeline_t *p,const uint16_t offsets[3],
                             uint8_t gate_qualified,uint8_t adc_qualified,
                             uint8_t throttle,uint8_t brake,uint8_t dashboard_valid,
                             uint32_t now_ms,uint16_t phase_u16,int16_t electrical_step_u16);
''','pipeline rolling prototype')
mph.write_text(s)

s=mpc.read_text()
anchor='void motor_pipeline_input(motor_pipeline_t *p, uint8_t throttle,\n'
rolling_pipeline=r'''uint8_t motor_pipeline_start_rolling(motor_pipeline_t *p,const uint16_t offsets[3],
                             uint8_t gate_ok,uint8_t adc_ok,
                             uint8_t throttle,uint8_t brake,uint8_t dash_ok,
                             uint32_t now_ms,uint16_t phase_u16,int16_t electrical_step_u16) {
    if (!p || !p->foc || p->powered || !p->io.gate_off || !p->io.arm ||
        !p->io.set_adc_pair || !p->io.set_pwm || !offsets || !gate_ok || !adc_ok ||
        !dash_ok || brake || !p->neutral_seen || p->fault_latched ||
        throttle<=THROTTLE_IDLE_MAX || electrical_step_u16>=0 ||
        !p->current_hard_limit_counts || !p->foc->motor_params_valid || !p->arr) {
        motor_pipeline_stop(p,MOTOR_STOP_QUALIFICATION);return 0u;
    }
    if(!foc_adc_contract_reset(&p->adc,1u)){
        motor_pipeline_stop(p,MOTOR_STOP_FAULT);return 0u;
    }
    for(unsigned i=0u;i<3u;i++)p->offsets[i]=offsets[i];
    p->last_sector=1u;p->next_sequence=1u;p->sample_divider=0u;
    p->last_dashboard_ms=now_ms;p->adc_qualified=1u;
    if(!foc_adc_contract_prepare(&p->adc,p->last_sector,1u,p->offsets,1u) ||
       !p->io.set_adc_pair(p->io_ctx,p->adc.expected_jsqr1,p->adc.expected_jsqr2) ||
       !p->io.set_pwm(p->io_ctx,(uint16_t)((p->arr+1u)/2u),
                      (uint16_t)((p->arr+1u)/2u),(uint16_t)((p->arr+1u)/2u)) ||
       !sensorless_control_start_rolling(p->foc,phase_u16,electrical_step_u16) ||
       !p->io.arm(p->io_ctx)) {
        motor_pipeline_stop(p,MOTOR_STOP_QUALIFICATION);return 0u;
    }
    p->powered=1u;p->neutral_seen=0u;p->last_stop_reason=MOTOR_STOP_NONE;
    return 1u;
}

'''
s=one(s,anchor,rolling_pipeline+anchor,'pipeline rolling function')
old='''    if (!p) return;
    if (p->powered && (!dashboard_valid || brake || throttle<=THROTTLE_IDLE_MAX)) {
'''
new='''    if (!p) return;
    if(!p->powered && dashboard_valid && !brake && throttle<=THROTTLE_IDLE_MAX)
        p->neutral_seen=1u;
    if (p->powered && (!dashboard_valid || brake || throttle<=THROTTLE_IDLE_MAX)) {
'''
s=one(s,old,new,'pipeline neutral latch')
mpc.write_text(s)

# Passive BEMF sampler and read-only FC telemetry on the actual G30 channels.
s=main.read_text()
s=one(s,'#include "g30_adc_proof.h"',
'''#include "g30_adc_proof.h"
#include "g30_rolling_bemf.h"
#include "g30_passive_bemf_adc.h"''','main rolling includes')
s=one(s,'#define FW_BUILD 0x0920u','#define FW_BUILD 0x1000u','v1000 build identity')
needle='#define COMM_INPUT_IDLE_MAX 3u\n'
s=one(s,needle,needle+'''#ifndef PASSIVE_BEMF_PROBE_ALLOWED
#define PASSIVE_BEMF_PROBE_ALLOWED 0
#endif
''','passive probe macro')
s=one(s,'static motor_pipeline_t g_motor_pipeline;\n',
'''static motor_pipeline_t g_motor_pipeline;
static g30_rolling_bemf_t g_rolling_bemf;
static g30_passive_bemf_adc_t g_passive_bemf_adc;
''','rolling globals')
s=one(s,
'''    gpio_cfg_nibble(GPIOA_BASE,5,0x0u);
''',
'''    gpio_cfg_nibble(GPIOA_BASE,5,0x0u);
    /* Stock/XBOT common BEMF inputs: PA6=A, PA7=B, PB1=C. */
    RCC_APB2ENR|=(1u<<3);
    gpio_cfg_nibble(GPIOA_BASE,6,0x0u);gpio_cfg_nibble(GPIOA_BASE,7,0x0u);
    gpio_cfg_nibble(GPIOB_BASE,1,0x0u);
''','BEMF analog GPIO')
s=one(s,
'    if(!g_vbus_pending && (uint32_t)(g_ms-g_vbus_start_ms)>=20u){',
'    if(!g_vbus_pending && !g_passive_bemf_adc.pending && (uint32_t)(g_ms-g_vbus_start_ms)>=20u){',
'ADC regular ownership')

anchor='static void adc_vbus_poll(void){'
probe=r'''static uint8_t passive_bemf_regular_start(void *ctx,uint8_t channel){
    (void)ctx;
    if(g_power_armed || g_vbus_pending || channel>17u)return 0u;
    ADC_SQR1=0u;ADC_SQR3=channel;ADC_SR&=~ADC_SR_EOC;ADC_CR2|=ADC_CR2_SWSTART;
    return 1u;
}
static uint8_t passive_bemf_regular_poll(void *ctx,uint16_t *value){
    (void)ctx;
    if(!(ADC_SR&ADC_SR_EOC))return 0u;
    if(value)*value=(uint16_t)ADC_DR;
    ADC_SR&=~ADC_SR_EOC;ADC_SQR3=1u;return 1u;
}
static void passive_bemf_regular_abort(void *ctx){
    (void)ctx;ADC_SR&=~ADC_SR_EOC;ADC_SQR3=1u;
}
static void passive_bemf_init(void){
    g30_rolling_bemf_config_t rc;g30_rolling_bemf_default_config(&rc);
    g30_rolling_bemf_init(&g_rolling_bemf,&rc);
    const g30_passive_bemf_adc_io_t io={passive_bemf_regular_start,
        passive_bemf_regular_poll,passive_bemf_regular_abort};
    g30_passive_bemf_adc_init(&g_passive_bemf_adc,io,0);
}
static void passive_bemf_poll(void){
#if PASSIVE_BEMF_PROBE_ALLOWED
    const uint8_t blocked=(uint8_t)(g_power_armed || iap_update_active() || iap_update_pending());
    g30_passive_bemf_adc_tick(&g_passive_bemf_adc,g_ms,blocked,g_vbus_pending);
    uint16_t abc[3];
    if(g30_passive_bemf_adc_take(&g_passive_bemf_adc,abc))
        g30_rolling_bemf_sample(&g_rolling_bemf,abc[0],abc[1],abc[2],DWT_CYCCNT);
#else
    (void)g_passive_bemf_adc;
#endif
}

'''
s=one(s,anchor,probe+anchor,'passive probe target binding')
s=one(s,
'''        const uint8_t armed=motor_pipeline_start(&g_motor_pipeline,
            (const uint16_t *)g_adc_offset,COMM_GATE_STOCK_PROFILE_READY &&
            COMM_CURRENT_SCALE_HW_VALID,COMM_ADC_TIMING_HW_VALID,
            g_dash_throttle,g_dash_brake,
            (uint8_t)(g_dash_seen && (uint32_t)(g_ms-g_dash_last_ms)<=COMM_DASH_STALE_MS),g_ms);''',
'''        const uint8_t dash_ok=(uint8_t)(g_dash_seen &&
            (uint32_t)(g_ms-g_dash_last_ms)<=COMM_DASH_STALE_MS);
        const uint32_t roll_now=DWT_CYCCNT;
        const uint8_t roll_ok=g30_rolling_bemf_ready(&g_rolling_bemf,roll_now);
        /* FO71-style rolling entry: no standstill ALIGN/OPEN fallback.
         * Release ADC1 regular ownership before the injected-current pipeline
         * can arm the bridge. */
        g30_passive_bemf_adc_cancel(&g_passive_bemf_adc);
        const uint8_t armed=roll_ok?motor_pipeline_start_rolling(&g_motor_pipeline,
            (const uint16_t *)g_adc_offset,COMM_GATE_STOCK_PROFILE_READY &&
            COMM_CURRENT_SCALE_HW_VALID,COMM_ADC_TIMING_HW_VALID,
            g_dash_throttle,g_dash_brake,dash_ok,g_ms,
            g_rolling_bemf.phase_u16,g_rolling_bemf.electrical_step_u16):0u;''',
'rolling E5 entry')
anchor='    case 0xDAu: case 0xDBu: case 0xDCu: case 0xDDu: case 0xDEu: case 0xDFu:'
page=r'''    case 0xFCu: { /* read-only FO70-derived rolling BEMF evidence */
        uint32_t now=DWT_CYCCNT;
        uint32_t age_cycles=g_rolling_bemf.last_edge_cycles?
            (uint32_t)(now-g_rolling_bemf.last_edge_cycles):0xffffffffu;
        uint16_t age_ms=(uint16_t)(age_cycles==0xffffffffu?65535u:
            (age_cycles/(SYSCLK_HZ/1000u)>65535u?65535u:age_cycles/(SYSCLK_HZ/1000u)));
        p[0]='R';p[1]='K';p[2]='1';p[3]='0';
        p[4]=(uint8_t)((g30_rolling_bemf_ready(&g_rolling_bemf,now)?1u:0u)|
             (g_rolling_bemf.direction_forward?2u:0u)|
             (g_passive_bemf_adc.fresh?4u:0u));
        p[5]=g_rolling_bemf.last_sector;
        put_u16(p+6,g30_rolling_bemf_speed_kmh10(&g_rolling_bemf,now));
        put_u16(p+8,g_rolling_bemf.span);put_u16(p+10,g_rolling_bemf.forward_edges);
        put_u16(p+12,g_rolling_bemf.reverse_edges);put_u16(p+14,g_rolling_bemf.bad_jumps);
        put_u16(p+16,g_rolling_bemf.phase_u16);put_u16(p+18,(uint16_t)g_rolling_bemf.electrical_step_u16);
        put_u16(p+20,g_rolling_bemf.a);put_u16(p+22,g_rolling_bemf.b);put_u16(p+24,g_rolling_bemf.c);
        put_u16(p+26,age_ms);n=28u;break;
    }
'''
s=one(s,anchor,page+anchor,'FC rolling page')
s=one(s,
'adc1_injected_init();tim1_pwm_and_adc_trigger_init();ninebot_link_init();iap_update_init();systick_init();irq_enable();',
'adc1_injected_init();tim1_pwm_and_adc_trigger_init();passive_bemf_init();ninebot_link_init();iap_update_init();systick_init();irq_enable();',
'passive probe init')
s=one(s,'        adc_vbus_poll();\n','        adc_vbus_poll();\n        passive_bemf_poll();\n','passive probe poll')
for req in ['#define COMM_CURRENT_SCALE_HW_VALID 0u','#define COMM_ADC_TIMING_HW_VALID 0u',
            'g_gate_kill_latched=1u;','TIM_BDTR(TIM1_BASE)&=(uint32_t)~D_GUARD_MOE;',
            'case 0xFAu: case 0xFBu:','case 0xFCu:','PB1 is BEMF_C',
            'motor_pipeline_start_rolling(&g_motor_pipeline']:
    if req not in s: raise RuntimeError('v0.10.0 inherited/ported contract missing: '+req)
main.write_text(s)

# Link all new modules in a new ELF-only target.
m=make.read_text()
mm=re.search(r'^(SRC\s*=.*)$',m,re.M)
if not mm: raise RuntimeError('v0.10.0 Makefile SRC line missing')
src_line=mm.group(1)
for f in ['src/g30_rolling_bemf.c','src/g30_passive_bemf_adc.c']:
    if f in src_line: raise RuntimeError('v0.10.0 Makefile already contains '+f)
src_line += ' src/g30_rolling_bemf.c src/g30_passive_bemf_adc.c'
m=m[:mm.start(1)]+src_line+m[mm.end(1):]
if 'xbot-rolling-locked:' in m: raise RuntimeError('v0.10.0 target exists')
m += r'''

# v0.10.0 FO69/FO70 source/link proof only. There is intentionally no BIN.
XBOT_ROLL_LOCKED_ELF = build/DeltaESC_G30D_v0_10_0_xbot_rolling_locked.elf
xbot-rolling-locked: $(XBOT_ROLL_LOCKED_ELF)
$(XBOT_ROLL_LOCKED_ELF): $(SRC) linker.ld | build
	$(CC) $(CFLAGS_COMMON) -DPOWER_STAGE_ARM_ALLOWED=0 -DSENSORLESS_RUN_ALLOWED=1 -DSENSORLESS_CLOSED_LOOP_ALLOWED=1 -DMOTOR_ID_BENCH_ALLOWED=0 -DPASSIVE_BEMF_PROBE_ALLOWED=1 $(SRC) $(LDFLAGS_COMMON) -Wl,--undefined=g30_rolling_bemf_sample -Wl,--undefined=g30_passive_bemf_adc_tick -Wl,--undefined=sensorless_control_start_rolling -Wl,--undefined=motor_pipeline_start_rolling -Wl,-Map=build/v0_10_0_xbot_rolling_locked.map -o $@
	$(OBJDUMP) -h $@ > build/v0_10_0_xbot_rolling_locked_sections.txt
'''
make.write_text(m)
print('PASS v0.10.0: FO69 takeover guards + FO70 passive rolling BEMF integrated, bridge remains compile-locked')
