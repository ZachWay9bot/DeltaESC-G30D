#!/usr/bin/env python3
from pathlib import Path
import sys
root=Path(sys.argv[1]).resolve()
main=(root/'src/main.c').read_text(); make=(root/'Makefile').read_text()
sc=(root/'src/sensorless_control.c').read_text(); sh=(root/'src/sensorless_control.h').read_text()
mpc=(root/'src/motor_pipeline.c').read_text(); mph=(root/'src/motor_pipeline.h').read_text()
flux=(root/'src/vesc_ebics_flux.c').read_text()
checks={
 'build-id':'#define FW_BUILD 0x1000u' in main,
 'hardware-current-still-unqualified':'#define COMM_CURRENT_SCALE_HW_VALID 0u' in main,
 'adc-timing-still-unqualified':'#define COMM_ADC_TIMING_HW_VALID 0u' in main,
 'terminal-moe-kill':'TIM_BDTR(TIM1_BASE)&=(uint32_t)~D_GUARD_MOE;' in main,
 'pb1-bemf-not-enable':'PB1 is BEMF_C' in main and 'gpio_cfg_nibble(GPIOB_BASE,1,0x0u)' in main,
 'passive-bemf-page':'case 0xFCu:' in main and "p[0]='R';p[1]='K';p[2]='1';p[3]='0';" in main,
 'passive-only-when-disarmed':'g30_passive_bemf_adc_tick(&g_passive_bemf_adc,g_ms,blocked,g_vbus_pending)' in main,
 'rolling-no-align-api':'sensorless_control_start_rolling' in sc and 'SENSORLESS_ROLL_VERIFY' in sc,
 'fo69-quiet-clamp':'clamp_events' in flux and 'rolling_clamp_start' in sc,
 'fo69-phase-guard':'ROLL_STEP_ABS_MAX_U16' in sc and 'ROLL_STEP_POS_MAX_U16' in sc,
 'zero-iq-verify':'s->state==SENSORLESS_ROLL_VERIFY)iq_ref=0' in sc,
 'bounded-iq-ramp':'ROLL_MAX_IQ_COUNTS' in sc and 'ROLL_IQ_RAMP_TICKS' in sc,
 'neutral-interlock':'neutral_seen' in mph and 'motor_pipeline_start_rolling' in mpc,
 'new-sources-linked':all(x in make for x in ['src/g30_rolling_bemf.c','src/g30_rolling_start.c','src/g30_passive_bemf_adc.c']),
 'no-xbot-bin-target':'XBOT_ROLL_LOCKED_BIN' not in make and 'xbot-rolling-locked:' in make,
 'physical-arm-zero':'-DPOWER_STAGE_ARM_ALLOWED=0' in make,
}
for k,v in checks.items():
    if not v: raise SystemExit('FAIL '+k)
block=make[make.index('XBOT_ROLL_LOCKED_ELF'):]
if '$(OBJCOPY)' in block: raise SystemExit('FAIL xbot rolling target creates binary')
for token in ['SENSORLESS_STOP = 0','SENSORLESS_ALIGN = 1','SENSORLESS_OPEN_LOOP = 2','SENSORLESS_HANDOVER = 3','SENSORLESS_CLOSED_LOOP = 4','SENSORLESS_FAULT = 5','SENSORLESS_ROLL_VERIFY = 6']:
    if token not in sh: raise SystemExit('FAIL state ABI '+token)
print('PASS v0.10.0 contract: FO69/FO70 features present, old ABI retained, no physical-arm or BIN release')
