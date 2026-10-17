#!/usr/bin/env python3
"""DeltaESC v0.10.1 rolling active RC.

Applied after v0.10.0 FO69/FO70 integration. This creates the first flashable
rolling-start sensorless FOC bench image. It deliberately keeps the global
COMM_* hardware-calibration flags false; the RC uses an explicit provisional
bench capability plus the existing raw-count / offset / timing guards.

FO70/FO69 profile adopted:
  R = 583000 uOhm
  L = 640000 nH
  lambda = 12100 uWb
  Flux->FOC frame offset ~= +117.15 electrical deg = 21327/65536 turn
  kick >= 3.0 km/h, forward raw rank step = 5
  rolling torque cap = 12 ADC current counts (already in v0.10.0)
"""
from pathlib import Path
import sys,re

target=Path(sys.argv[1]).resolve()
main=target/'src/main.c'
sc=target/'src/sensorless_control.c'
make=target/'Makefile'

def one(s,old,new,label):
    n=s.count(old)
    if n!=1:
        raise RuntimeError(f'v0.10.1 {label}: expected one anchor, got {n}: {old[:120]!r}')
    return s.replace(old,new,1)

# -------- main.c: explicit RC capability, profile and runtime guards --------
s=main.read_text()
s=one(s,'#define FW_BUILD 0x1000u','#define FW_BUILD 0x1010u','build id')

needle='''#ifndef PASSIVE_BEMF_PROBE_ALLOWED
#define PASSIVE_BEMF_PROBE_ALLOWED 0
#endif
'''
s=one(s,needle,needle+'''#ifndef ROLLING_SENSORLESS_RC_ALLOWED
#define ROLLING_SENSORLESS_RC_ALLOWED 0
#endif
#define FO70_R_UOHM 583000u
#define FO70_L_NH 640000u
#define FO70_FLUX_UWB 12100u
#define FO70_FRAME_OFFSET_U16 21327
#define FO70_RUN_CURRENT_MA 600u
''','rc capability/profile')

s=one(s,
'''#define HARD_OC_COUNTS                700u''',
'''#if ROLLING_SENSORLESS_RC_ALLOWED
#define HARD_OC_COUNTS                 96u /* ~4.8 A on stock nominal count model; raw-count trip */
#else
#define HARD_OC_COUNTS                700u
#endif''','rc hard OC')

s=one(s,
'''#if POWER_STAGE_ARM_ALLOWED
#error "v0.9.20 SENSORLESS-LOCKED: physical gate arm forbidden"
#endif''',
'''#if POWER_STAGE_ARM_ALLOWED && !ROLLING_SENSORLESS_RC_ALLOWED
#error "v0.10.1: physical gate arm permitted only by explicit rolling RC target"
#endif''','arm compile gate')

s=one(s,
'''#if POWER_STAGE_ARM_ALLOWED
#if !COMM_CURRENT_SCALE_HW_VALID || !COMM_ADC_TIMING_HW_VALID
#error "DRV126 gate profile ready; powered build still requires valid ADC timing and current scaling"
#endif
#endif''',
'''/* Production still requires fully qualified calibration flags. The explicit
 * rolling RC instead uses the stock DRV126 nominal count model plus SAME-TICK
 * runtime window/offset guards and a much lower raw-count trip. */
#if POWER_STAGE_ARM_ALLOWED && !ROLLING_SENSORLESS_RC_ALLOWED
#if !COMM_CURRENT_SCALE_HW_VALID || !COMM_ADC_TIMING_HW_VALID
#error "DRV126 gate profile ready; powered build still requires valid ADC timing and current scaling"
#endif
#endif''','qualification compile gate')

# Require real boot offsets for the RC path, and allow the provisional path
# without pretending COMM_* is production-calibrated.
s=one(s,
'''    if (!COMM_GATE_STOCK_PROFILE_READY || g_safety_latched ||
        (!g_id_bench_requested && (!COMM_CURRENT_SCALE_HW_VALID || !COMM_ADC_TIMING_HW_VALID)) ||''',
'''    if (!COMM_GATE_STOCK_PROFILE_READY || g_safety_latched ||
        (ROLLING_SENSORLESS_RC_ALLOWED &&
         (!g_offset_cal_valid || !g_adc2_ch4_offset_valid || !g_adc2_ch5_offset_valid)) ||
        (!g_id_bench_requested && !ROLLING_SENSORLESS_RC_ALLOWED &&
         (!COMM_CURRENT_SCALE_HW_VALID || !COMM_ADC_TIMING_HW_VALID)) ||''','motor arm RC bypass')

s=one(s,
'''            ((!COMM_CURRENT_SCALE_HW_VALID || !COMM_ADC_TIMING_HW_VALID) &&
             !(MOTOR_ID_BENCH_ALLOWED && g_id_bench_requested))) ok=0u;''',
'''            ((!COMM_CURRENT_SCALE_HW_VALID || !COMM_ADC_TIMING_HW_VALID) &&
             !(MOTOR_ID_BENCH_ALLOWED && g_id_bench_requested) &&
             !ROLLING_SENSORLESS_RC_ALLOWED)) ok=0u;''','gate safety RC bypass')

s=one(s,
'''        sample.sample_window_valid=(uint8_t)(COMM_ADC_TIMING_HW_VALID &&
            offset_pair_ok && g30_adc_current_flags(&current_adc_state)==0u);''',
'''        sample.sample_window_valid=(uint8_t)(
            (COMM_ADC_TIMING_HW_VALID || ROLLING_SENSORLESS_RC_ALLOWED) &&
            offset_pair_ok && g30_adc_current_flags(&current_adc_state)==0u);''',
'timing runtime guard')

s=one(s,
'''            (const uint16_t *)g_adc_offset,COMM_GATE_STOCK_PROFILE_READY &&
            COMM_CURRENT_SCALE_HW_VALID,COMM_ADC_TIMING_HW_VALID,
            g_dash_throttle,g_dash_brake,dash_ok,g_ms,''',
'''            (const uint16_t *)g_adc_offset,COMM_GATE_STOCK_PROFILE_READY &&
            (COMM_CURRENT_SCALE_HW_VALID || ROLLING_SENSORLESS_RC_ALLOWED),
            (COMM_ADC_TIMING_HW_VALID || ROLLING_SENSORLESS_RC_ALLOWED),
            g_dash_throttle,g_dash_brake,dash_ok,g_ms,''','rolling E5 qualification')

# Explicit FO70 calibration in passive rolling detector. This offset converts
# the physical PM-flux alpha/beta frame into the proven FOC control frame.
s=one(s,
'''static void passive_bemf_init(void){
    g30_rolling_bemf_config_t rc;g30_rolling_bemf_default_config(&rc);
    g30_rolling_bemf_init(&g_rolling_bemf,&rc);''',
'''static void passive_bemf_init(void){
    g30_rolling_bemf_config_t rc;g30_rolling_bemf_default_config(&rc);
#if ROLLING_SENSORLESS_RC_ALLOWED
    rc.wheel_circ_mm=763u;
    rc.pole_pairs=15u;
    rc.forward_raw_step=5u;
    rc.min_speed_kmh10=30u;
    rc.phase_offset_u16=(int16_t)FO70_FRAME_OFFSET_U16;
#endif
    g30_rolling_bemf_init(&g_rolling_bemf,&rc);''','FO70 rolling profile')

# Load the already measured motor model into the controller at boot for this RC.
s=one(s,
'''irq_disable();power_hold_init();clock_64mhz_hsi();dwt_init();gate_pins_to_safe_inputs();uart1_debug_init();sensorless_control_init(&ctrl);
    const motor_pipeline_io_t motor_io={motor_hw_arm,motor_hw_gate_off,''',
'''irq_disable();power_hold_init();clock_64mhz_hsi();dwt_init();gate_pins_to_safe_inputs();uart1_debug_init();sensorless_control_init(&ctrl);
#if ROLLING_SENSORLESS_RC_ALLOWED
    if(!sensorless_control_set_motor_params(&ctrl,FO70_R_UOHM,FO70_L_NH,FO70_FLUX_UWB) ||
       !sensorless_control_set_run_current_ma(&ctrl,FO70_RUN_CURRENT_MA)) {
        g_safety_latched=0xA020u;
    }
    sensorless_control_set_phase_offset(&ctrl,(int16_t)FO70_FRAME_OFFSET_U16);
#endif
    const motor_pipeline_io_t motor_io={motor_hw_arm,motor_hw_gate_off,''','FO70 motor model init')

for req in [
    '#define COMM_CURRENT_SCALE_HW_VALID 0u',
    '#define COMM_ADC_TIMING_HW_VALID 0u',
    'ROLLING_SENSORLESS_RC_ALLOWED',
    'FO70_FRAME_OFFSET_U16 21327',
    'HARD_OC_COUNTS                 96u',
    'motor_pipeline_start_rolling(&g_motor_pipeline',
    'g_gate_kill_latched=1u;',
    'TIM_BDTR(TIM1_BASE)&=(uint32_t)~D_GUARD_MOE;'
]:
    if req not in s:
        raise RuntimeError('v0.10.1 inherited/RC contract missing: '+req)
main.write_text(s)

# -------- sensorless_control.c: correct frame-offset seeding --------
s=sc.read_text()
s=one(s,
'''    const int32_t lambda=(int32_t)(s->motor_flux_uwb*1000u);
    const int32_t fa=(int32_t)(((int64_t)lambda*cos_q15(phase_u16))>>15);
    const int32_t fb=(int32_t)(((int64_t)lambda*sin_q15(phase_u16))>>15);
    s->vesc_flux.x_alpha_nwb=fa;s->vesc_flux.x_beta_nwb=fb;''',
'''    const int32_t lambda=(int32_t)(s->motor_flux_uwb*1000u);
    /* phase_u16 is already in FOC control frame. The observer's internal
     * x/flux vector is physical alpha/beta, therefore remove the learned
     * Flux->FOC frame offset before seeding. Observer_update() adds it back
     * after atan2. This avoids applying the ~117 deg calibration twice. */
    const uint16_t seed_flux_phase=(uint16_t)(phase_u16-s->phase_offset_u16);
    const int32_t fa=(int32_t)(((int64_t)lambda*cos_q15(seed_flux_phase))>>15);
    const int32_t fb=(int32_t)(((int64_t)lambda*sin_q15(seed_flux_phase))>>15);
    s->vesc_flux.x_alpha_nwb=fa;s->vesc_flux.x_beta_nwb=fb;''','flux seed frame correction')
sc.write_text(s)

# -------- Makefile: explicit flashable RC target --------
m=make.read_text()
if 'rolling-active-rc:' in m:
    raise RuntimeError('v0.10.1 target already exists')
m += r'''

# v0.10.1 first active rolling sensorless RC.
# Produces a BIN intentionally; this is bench/current-limited bring-up, not ride release.
ROLLING_ACTIVE_RC_ELF = build/DeltaESC_G30D_v0_10_1_rolling_active_rc.elf
ROLLING_ACTIVE_RC_BIN = build/DeltaESC_G30D_v0_10_1_rolling_active_rc.bin
rolling-active-rc: $(ROLLING_ACTIVE_RC_BIN)
$(ROLLING_ACTIVE_RC_ELF): $(SRC) linker.ld | build
	$(CC) $(CFLAGS_COMMON) -DPOWER_STAGE_ARM_ALLOWED=1 -DSENSORLESS_RUN_ALLOWED=1 -DSENSORLESS_CLOSED_LOOP_ALLOWED=1 -DMOTOR_ID_BENCH_ALLOWED=0 -DPASSIVE_BEMF_PROBE_ALLOWED=1 -DROLLING_SENSORLESS_RC_ALLOWED=1 $(SRC) $(LDFLAGS_COMMON) -Wl,--undefined=g30_rolling_bemf_sample -Wl,--undefined=g30_passive_bemf_adc_tick -Wl,--undefined=sensorless_control_start_rolling -Wl,--undefined=motor_pipeline_start_rolling -Wl,-Map=build/v0_10_1_rolling_active_rc.map -o $@
	$(OBJDUMP) -h $@ > build/v0_10_1_rolling_active_rc_sections.txt
$(ROLLING_ACTIVE_RC_BIN): $(ROLLING_ACTIVE_RC_ELF)
	$(OBJCOPY) -O binary $< $@
'''
make.write_text(m)
print('PASS v0.10.1 active RC overlay: FO70 profile + rolling-only E5 + provisional runtime-guarded power path')
