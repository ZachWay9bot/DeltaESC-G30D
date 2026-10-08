#!/usr/bin/env python3
"""Add a phone-visible commissioning safety gate to v0.8.0 without enabling PWM gates."""
from pathlib import Path
import sys
root=Path(sys.argv[1]).resolve()
repo=Path(__file__).resolve().parent

def one(s,old,new):
    n=s.count(old)
    if n!=1: raise RuntimeError(f'expected exactly one marker ({n}): {old[:120]!r}')
    return s.replace(old,new,1)

for name in ('commissioning_guard.c','commissioning_guard.h'):
    (root/'src'/name).write_bytes((repo/'src'/name).read_bytes())

p=root/'src/main.c'; s=p.read_text()
s=one(s,'#include "ble_motor_probe.h"','#include "ble_motor_probe.h"\n#include "commissioning_guard.h"')
s=one(s,'#define FW_BUILD 0x0800u','''#define FW_BUILD 0x0801u
#define COMM_DASH_STALE_MS 250u
#define COMM_INPUT_IDLE_MAX 3u
/* Hardware qualification flags deliberately remain false in v0.8.1. */
#define COMM_CURRENT_SCALE_HW_VALID 0u
#define COMM_GATE_HW_VALID 0u
#define COMM_ADC_TIMING_HW_VALID 0u''')
s=one(s,'static volatile uint8_t g_offset_cal_active;','static volatile uint8_t g_offset_cal_active;\nstatic volatile uint8_t g_offset_cal_valid;')
s=one(s,'g_offset_cal_active=0u;g_last_action_status=ACT_OK;','g_offset_cal_active=0u;g_offset_cal_valid=1u;g_last_action_status=ACT_OK;')
s=one(s,'static uint16_t current_fault_code(void){if(g_safety_latched)return (uint16_t)g_safety_latched;return (uint16_t)ctrl.fault;}','''static uint16_t current_fault_code(void){if(g_safety_latched)return (uint16_t)g_safety_latched;return (uint16_t)ctrl.fault;}

static uint16_t commissioning_guard_mask(void){
    commissioning_guard_input_t in;
    in.now_ms=g_ms;in.adc_last_ms=g_adc_last_ms;in.dash_last_ms=g_dash_last_ms;
    in.adc_stale_ms=ADC_STALE_MS;in.dash_stale_ms=COMM_DASH_STALE_MS;
    in.abs_current_counts=g_last_abs_current;in.idle_current_limit_counts=ARM_IDLE_CURRENT_COUNTS;
    in.vbus_raw=g_adc_raw[3];in.vbus_raw_min=50u;in.vbus_raw_max=4050u;
    in.offset_valid=g_offset_cal_valid;in.params_valid=ctrl.motor_params_valid;in.dash_seen=g_dash_seen;
    in.throttle=g_dash_throttle;in.brake=g_dash_brake;in.input_idle_max=COMM_INPUT_IDLE_MAX;
    in.safety_latched=(uint8_t)(g_safety_latched!=0u);
    in.iap_busy=(uint8_t)(iap_update_pending()||iap_update_active());
    in.poweroff_pending=g_poweroff_requested;
    in.control_stopped=(uint8_t)(ctrl.state==SENSORLESS_STOP && !ctrl.drive_request && !g_power_armed);
    in.current_scale_hw_valid=COMM_CURRENT_SCALE_HW_VALID;
    in.gate_hw_valid=COMM_GATE_HW_VALID;
    in.adc_timing_hw_valid=COMM_ADC_TIMING_HW_VALID;
    return commissioning_guard_eval(&in);
}''')
s=one(s,'case 0xD9u: put_u16(p,current_fault_code());put_u16(p+2,diag_flags());put_u16(p+4,HARD_OC_COUNTS);n=6u;break;','''case 0xD9u:
        put_u16(p,current_fault_code());put_u16(p+2,diag_flags());put_u16(p+4,HARD_OC_COUNTS);
        put_u16(p+6,commissioning_guard_mask());p[8]=g_offset_cal_valid;p[9]=ctrl.motor_params_valid;
        p[10]=g_phase_map_valid;p[11]=g_last_action_status;put_u16(p+12,g_adc_raw[3]);put_u16(p+14,g_cfg_test_current_ma);n=16u;break;''')
s=one(s,'if(cmd>=0xE1u&&cmd<=0xE4u){action_ack(f,cmd,ACT_UNSUPPORTED);return;}','''if(cmd==0xE1u){
        uint16_t guard=commissioning_guard_mask();
        action_ack(f,cmd,guard?ACT_UNSAFE:ACT_OK);return;
    }
    if(cmd>=0xE2u&&cmd<=0xE4u){action_ack(f,cmd,ACT_UNSUPPORTED);return;}''')
s=one(s,'if(g_offset_cal_active){action_ack(f,cmd,ACT_BUSY);return;}\n        if(!g_power_armed&&!power_stage_arm_zero_vector()){action_ack(f,cmd,ACT_UNSAFE);return;}','''if(g_offset_cal_active){action_ack(f,cmd,ACT_BUSY);return;}
        if(commissioning_guard_mask()!=0u){action_ack(f,cmd,ACT_UNSAFE);return;}
        if(!g_power_armed&&!power_stage_arm_zero_vector()){action_ack(f,cmd,ACT_UNSAFE);return;}''')
s=one(s,'v0.8.0 integrated motor candidate must NEVER arm a physical power stage','v0.8.1 BLE commissioning audit must NEVER arm a physical power stage')
s=one(s,'v0.8.0 DASHBLE PARAMETERIZED MOTOR CORE GATES OFF','v0.8.1 DASHBLE COMMISSIONING GUARD GATES OFF')
s=one(s,'AUTO R/L/flux/phase identification E1-E4 intentionally locked until active-vector bench validation.','E1 = read-only commissioning preflight ACK; E2-E4 remain locked. D9 bytes 6..15 expose exact guard blockers.')
p.write_text(s)

p=root/'Makefile'; s=p.read_text()
s=one(s,'src/ble_motor_probe.c','src/ble_motor_probe.c src/commissioning_guard.c')
s=s.replace('v0_8_0','v0_8_1')
p.write_text(s)
print('v0.8.1: DashBLE commissioning guard + D9 blockers; hardware gates remain compile-disabled')
