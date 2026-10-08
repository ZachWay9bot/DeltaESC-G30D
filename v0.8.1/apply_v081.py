#!/usr/bin/env python3
"""Apply v0.8.1 transactional DashBLE motor configuration to reconstructed v0.8.0."""
from pathlib import Path
import shutil, sys
root=Path(sys.argv[1]).resolve()
base=Path(__file__).resolve().parent

def sub1(s,old,new):
    n=s.count(old)
    if n!=1: raise RuntimeError(f'expected one marker, got {n}: {old[:120]!r}')
    return s.replace(old,new,1)

for rel in ('src/motor_config_txn.c','src/motor_config_txn.h','tools/motor_config_txn_test.c','tools/test_motor_config_txn.py','tools/preflight_v081.py'):
    dst=root/rel; dst.parent.mkdir(parents=True,exist_ok=True); shutil.copyfile(base/rel,dst)

p=root/'src/main.c'; s=p.read_text()
s=sub1(s,'#include "ble_motor_probe.h"','#include "ble_motor_probe.h"\n#include "motor_config_txn.h"')
s=sub1(s,'#define FW_BUILD 0x0800u','#define FW_BUILD 0x0801u')
s=sub1(s,'v0.8.0 integrated motor candidate must NEVER arm a physical power stage','v0.8.1 BLE transactional config must NEVER arm a physical power stage')
s=sub1(s,'static uint8_t g_cfg_dirty;','static uint8_t g_cfg_dirty;\nstatic motor_config_txn_t g_motor_cfg;')
s=sub1(s,
 'case 0xD9u: put_u16(p,current_fault_code());put_u16(p+2,diag_flags());put_u16(p+4,HARD_OC_COUNTS);n=6u;break;',
 'case 0xD9u: put_u16(p,current_fault_code());put_u16(p+2,diag_flags());put_u16(p+4,HARD_OC_COUNTS);put_i16(p+6,g_cfg_phase_offset);put_u16(p+8,g_cfg_test_current_ma);p[10]=g_motor_cfg.pending_mask;p[11]=g_motor_cfg.active_valid;n=12u;break;')
start=s.index('/* Unlike the v0.7.2 placeholder fields')
end=s.index('static uint8_t handle_stock_dashboard',start)
block=r'''/* v0.8.1 motor configuration is transactional and RAM-only.
 * F0..F4 require MAGIC_UNLOCK and only stage a complete shadow tuple.
 * F5 + MAGIC_UNLOCK atomically applies the tuple to the observer. Partial
 * tuples cannot perturb the running control state. Nothing is persisted to
 * STM32 flash in this source-only release. */
static void sync_active_config_from_txn(void){
    g_cfg_r_uohm=g_motor_cfg.active.r_uohm;
    g_cfg_l_nh=g_motor_cfg.active.l_nh;
    g_cfg_flux_uwb=g_motor_cfg.active.flux_uwb;
    g_cfg_phase_offset=g_motor_cfg.active.phase_offset;
    g_cfg_test_current_ma=g_motor_cfg.active.test_current_ma;
}

static uint8_t commit_motor_config(void){
    if(!motor_config_pending_complete(&g_motor_cfg))return 0u;
    motor_config_values_t v=g_motor_cfg.pending;
    /* The transaction module already range-validates every field. Mask the ADC
       ISR as well so it can never observe R/L/flux from one tuple and current/
       phase from another, even if this code is reused by a future active build. */
    irq_disable();
    uint8_t ok=(uint8_t)(sensorless_control_set_motor_params(&ctrl,v.r_uohm,v.l_nh,v.flux_uwb) &&
                         sensorless_control_set_run_current_ma(&ctrl,v.test_current_ma));
    if(ok){
        sensorless_control_set_phase_offset(&ctrl,v.phase_offset);
        ok=motor_config_commit(&g_motor_cfg);
        if(ok){sync_active_config_from_txn();g_cfg_dirty=1u;}
    }
    irq_enable();
    return ok;
}

static void app_write_config(const ninebot_frame_t *f){
    uint8_t cmd=f->arg;
    if(g_power_armed||ctrl.drive_request||ctrl.state!=SENSORLESS_STOP){action_ack(f,cmd,ACT_UNSAFE);return;}
    if(!has_magic(f)){action_ack(f,cmd,ACT_BAD_MAGIC);return;}
    if(cmd==0xF0u||cmd==0xF1u||cmd==0xF2u){
        if(f->payload_len<6u){action_ack(f,cmd,ACT_BAD_RANGE);return;}
        if(!motor_config_stage_u32(&g_motor_cfg,cmd,get_u32(f->payload+2))){action_ack(f,cmd,ACT_BAD_RANGE);return;}
        action_ack(f,cmd,ACT_RAM_ONLY);return;
    }
    if(cmd==0xF3u){
        if(f->payload_len<4u){action_ack(f,cmd,ACT_BAD_RANGE);return;}
        (void)motor_config_stage_i16(&g_motor_cfg,get_i16(f->payload+2));
        action_ack(f,cmd,ACT_RAM_ONLY);return;
    }
    if(cmd==0xF4u){
        if(f->payload_len<4u){action_ack(f,cmd,ACT_BAD_RANGE);return;}
        if(!motor_config_stage_u16(&g_motor_cfg,get_u16(f->payload+2))){action_ack(f,cmd,ACT_BAD_RANGE);return;}
        action_ack(f,cmd,ACT_RAM_ONLY);return;
    }
    if(cmd==0xF5u){
        if(f->payload_len==3u && f->payload[2]==0u){motor_config_abort(&g_motor_cfg);action_ack(f,cmd,ACT_OK);return;}
        if(f->payload_len!=2u){action_ack(f,cmd,ACT_BAD_RANGE);return;}
        if(!commit_motor_config()){action_ack(f,cmd,ACT_BAD_RANGE);return;}
        action_ack(f,cmd,ACT_RAM_ONLY);return;
    }
    action_ack(f,cmd,ACT_UNSUPPORTED);
}

'''
s=s[:start]+block+s[end:]
s=sub1(s,'sensorless_control_init(&ctrl);','sensorless_control_init(&ctrl);motor_config_txn_init(&g_motor_cfg,g_cfg_test_current_ma);')
s=sub1(s,'v0.8.0 DASHBLE PARAMETERIZED MOTOR CORE GATES OFF','v0.8.1 DASHBLE TRANSACTIONAL MOTOR CONFIG GATES OFF')
p.write_text(s)

p=root/'Makefile'; s=p.read_text()
s=sub1(s,'src/ble_motor_probe.c','src/ble_motor_probe.c src/motor_config_txn.c')
s=s.replace('v0_8_0','v0_8_1')
p.write_text(s)
shutil.copyfile(base/'README.md',root/'README.md')
print('v0.8.1: transactional F0-F5 RAM configuration, gates off')
