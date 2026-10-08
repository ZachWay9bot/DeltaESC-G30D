#!/usr/bin/env python3
"""Merge protected staged F0-F5 into v0.8.3 dual-ADC/commissioning branch.
SOURCE ONLY. All six bridge pins stay compile-disabled; no flash images.
"""
from pathlib import Path
import sys,shutil
root=Path(sys.argv[1]).resolve(); here=Path(__file__).resolve().parent
def one(s,a,b):
    n=s.count(a)
    if n!=1:raise RuntimeError(f"expected one source anchor, got {n}: {a[:100]!r}")
    return s.replace(a,b,1)
for name in ("motor_config_txn.c","motor_config_txn.h"):
    shutil.copyfile(here/"src"/name,root/"src"/name)
p=root/"src/main.c";s=p.read_text()
s=one(s,'#include "stock_dual_adc_plan.h"','#include "stock_dual_adc_plan.h"\n#include "motor_config_txn.h"')
s=one(s,'#define FW_BUILD 0x0803u','#define FW_BUILD 0x0804u')
s=one(s,'v0.8.3 stock dual-ADC acquisition audit must NEVER arm a physical power stage',
        'v0.8.4 transactional commissioning must NEVER arm a physical power stage')
s=one(s,'static uint8_t g_cfg_dirty;','static uint8_t g_cfg_dirty;\nstatic motor_config_txn_t g_motor_cfg;')
s=one(s,'case 0xDAu: case 0xDBu: case 0xDCu: case 0xDDu: case 0xDEu: case 0xDFu:',
'''case 0xE7u: /* read-only status of staged motor configuration */
        p[0]=g_motor_cfg.pending_mask;
        p[1]=g_motor_cfg.active_valid;
        p[2]=ctrl.motor_params_valid;
        p[3]=g_power_armed;
        put_u16(p+4,g_cfg_test_current_ma);
        put_u16(p+6,commissioning_guard_mask());
        put_u32(p+8,g_cfg_r_uohm);
        put_u32(p+12,g_cfg_l_nh);
        n=16u;break;
    case 0xDAu: case 0xDBu: case 0xDCu: case 0xDDu: case 0xDEu: case 0xDFu:''')
start=s.index('/* Unlike the v0.7.2 placeholder fields')
end=s.index('static uint8_t handle_stock_dashboard',start)
block='''/* Two-phase RAM transaction. The previous F0/F1/F2 direct writes were
 * unsafe for the observer: partial tuples can alter the motor model. */
static void sync_active_config_from_txn(void) {
    g_cfg_r_uohm=g_motor_cfg.active.r_uohm;
    g_cfg_l_nh=g_motor_cfg.active.l_nh;
    g_cfg_flux_uwb=g_motor_cfg.active.flux_uwb;
    g_cfg_phase_offset=g_motor_cfg.active.phase_offset;
    g_cfg_test_current_ma=g_motor_cfg.active.test_current_ma;
}
static uint8_t commit_motor_config(void) {
    if(!motor_config_pending_complete(&g_motor_cfg))return 0u;
    const motor_config_values_t v=g_motor_cfg.pending;
    irq_disable();
    uint8_t ok=(uint8_t)(sensorless_control_set_motor_params(&ctrl,v.r_uohm,v.l_nh,v.flux_uwb) &&
                         sensorless_control_set_run_current_ma(&ctrl,v.test_current_ma));
    if(ok) {
        sensorless_control_set_phase_offset(&ctrl,v.phase_offset);
        ok=motor_config_commit(&g_motor_cfg);
        if(ok){sync_active_config_from_txn();g_cfg_dirty=1u;}
    }
    irq_enable();
    return ok;
}
static void app_write_config(const ninebot_frame_t *f) {
    const uint8_t cmd=f->arg;
    /* NEVER stage while drive, ADC calibration, IAP or poweroff is active. */
    if(g_power_armed || ctrl.drive_request || ctrl.state!=SENSORLESS_STOP ||
       g_offset_cal_active || g_poweroff_requested || iap_update_active() ||
       iap_update_pending()) {action_ack(f,cmd,ACT_UNSAFE);return;}
    if(!has_magic(f)){action_ack(f,cmd,ACT_BAD_MAGIC);return;}
    if(cmd>=0xF0u&&cmd<=0xF2u) {
        if(f->payload_len!=6u || !motor_config_stage_u32(&g_motor_cfg,cmd,get_u32(f->payload+2))) {
            action_ack(f,cmd,ACT_BAD_RANGE);return;
        }
        action_ack(f,cmd,ACT_RAM_ONLY);return;
    }
    if(cmd==0xF3u) {
        if(f->payload_len!=4u){action_ack(f,cmd,ACT_BAD_RANGE);return;}
        (void)motor_config_stage_i16(&g_motor_cfg,get_i16(f->payload+2));
        action_ack(f,cmd,ACT_RAM_ONLY);return;
    }
    if(cmd==0xF4u) {
        if(f->payload_len!=4u || !motor_config_stage_u16(&g_motor_cfg,get_u16(f->payload+2))) {
            action_ack(f,cmd,ACT_BAD_RANGE);return;
        }
        action_ack(f,cmd,ACT_RAM_ONLY);return;
    }
    if(cmd==0xF5u) {
        if(f->payload_len==3u && f->payload[2]==0u) {
            motor_config_abort(&g_motor_cfg);action_ack(f,cmd,ACT_OK);return;
        }
        if(f->payload_len!=2u || !commit_motor_config()) {
            action_ack(f,cmd,ACT_BAD_RANGE);return;
        }
        action_ack(f,cmd,ACT_RAM_ONLY);return;
    }
    action_ack(f,cmd,ACT_UNSUPPORTED);
}

'''
s=s[:start]+block+s[end:]
s=one(s,'sensorless_control_init(&ctrl);motor_probe_init(&ctrl)', 'sensorless_control_init(&ctrl);motor_config_txn_init(&g_motor_cfg,g_cfg_test_current_ma);motor_probe_init(&ctrl)') if False else s
s=one(s,'sensorless_control_init(&ctrl);motor_probe_init();',
      'sensorless_control_init(&ctrl);motor_config_txn_init(&g_motor_cfg,g_cfg_test_current_ma);motor_probe_init();')
s=one(s,'v0.8.3 STOCK DUAL ADC ACQUISITION PLAN GATES OFF','v0.8.4 TRANSACTIONAL PARAMS + DUAL ADC PLAN GATES OFF')
assert '#if POWER_STAGE_ARM_ALLOWED || SENSORLESS_RUN_ALLOWED' in s
assert 'case 0xE7u:' in s and 'COMM_ADC_TIMING_HW_VALID 0u' in s
p.write_text(s)
p=root/"Makefile";m=p.read_text()
m=one(m,'src/stock_dual_adc_plan.c','src/stock_dual_adc_plan.c src/motor_config_txn.c')
m=m.replace('v0_8_3','v0_8_4')
assert 'all: safe\n' in m and 'active: ' not in m
assert '-DPOWER_STAGE_ARM_ALLOWED=0 -DSENSORLESS_RUN_ALLOWED=0' in m
p.write_text(m)
print("v0.8.4: transactional F0-F5 + E7 read-only status; dual ADC plan, hardware guard and BLE intact; gates OFF")
