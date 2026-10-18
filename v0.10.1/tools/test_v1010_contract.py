#!/usr/bin/env python3
from pathlib import Path
import sys
root=Path(sys.argv[1]).resolve()
main=(root/'src/main.c').read_text()
sc=(root/'src/sensorless_control.c').read_text()
make=(root/'Makefile').read_text()
checks={
 'build-id':'#define FW_BUILD 0x1010u' in main,
 'production-flags-stay-false':'#define COMM_CURRENT_SCALE_HW_VALID 0u' in main and '#define COMM_ADC_TIMING_HW_VALID 0u' in main,
 'explicit-rc-capability':'ROLLING_SENSORLESS_RC_ALLOWED' in main,
 'fo70-R':'#define FO70_R_UOHM 583000u' in main,
 'fo70-L':'#define FO70_L_NH 640000u' in main,
 'fo70-flux':'#define FO70_FLUX_UWB 12100u' in main,
 'fo70-frame':'#define FO70_FRAME_OFFSET_U16 21327' in main,
 'fo70-kick':'rc.min_speed_kmh10=30u' in main and 'rc.forward_raw_step=5u' in main and 'rc.pole_pairs=15u' in main,
 'boot-motor-model':'sensorless_control_set_motor_params(&ctrl,FO70_R_UOHM,FO70_L_NH,FO70_FLUX_UWB)' in main,
 'boot-frame-offset':'sensorless_control_set_phase_offset(&ctrl,(int16_t)FO70_FRAME_OFFSET_U16)' in main,
 'raw-trip-96':'HARD_OC_COUNTS                 96u' in main,
 'runtime-window-guard':'COMM_ADC_TIMING_HW_VALID || ROLLING_SENSORLESS_RC_ALLOWED' in main and 'g30_adc_current_flags(&current_adc_state)==0u' in main,
 'real-offsets-required':'!g_offset_cal_valid || !g_adc2_ch4_offset_valid || !g_adc2_ch5_offset_valid' in main,
 'rolling-only-E5':'motor_pipeline_start_rolling(&g_motor_pipeline' in main and 'motor_pipeline_start(&g_motor_pipeline' not in main,
 'gate-kill-retained':'g_gate_kill_latched=1u;' in main and 'TIM_BDTR(TIM1_BASE)&=(uint32_t)~D_GUARD_MOE;' in main,
 'frame-seed-corrected':'seed_flux_phase=(uint16_t)(phase_u16-s->phase_offset_u16)' in sc,
 'active-bin-target':'ROLLING_ACTIVE_RC_BIN' in make and 'rolling-active-rc:' in make and '$(OBJCOPY) -O binary' in make,
 'active-compile-flags':'-DPOWER_STAGE_ARM_ALLOWED=1' in make and '-DROLLING_SENSORLESS_RC_ALLOWED=1' in make,
 'locked-target-retained':'xbot-rolling-locked:' in make,
}
for k,v in checks.items():
    if not v: raise SystemExit('FAIL '+k)
print('PASS v0.10.1 contract: FO70 profile, frame-correct seed, runtime guarded active RC, terminal kill retained')
