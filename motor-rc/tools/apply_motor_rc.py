#!/usr/bin/env python3
"""Motor RC: isolate E7 measurement coasting from terminal faults on v0.9.17.

Apply only AFTER v116 and v117 to reconstructed v0.9.14 source.
Retains same authenticated Ninebot E7/E6 protocol and normal drive OFF.
Full gate implementation is experimental, NOT electrically qualified.
"""
from pathlib import Path
import sys
target=Path(sys.argv[1]).resolve()
main=target/'src/main.c'
make=target/'Makefile'
s=main.read_text()
def once(old,new):
    global s
    n=s.count(old)
    if n!=1:
        raise RuntimeError("Motor RC main.c drift: "+repr(old[:110])+" hits="+str(n))
    s=s.replace(old,new,1)

# v0.9.16/17 banned E7: replace the bans while still preventing normal drive.
once('''#define FW_BUILD 0x0917u
#if MOTOR_ID_BENCH_ALLOWED || POWER_STAGE_ARM_ALLOWED || SENSORLESS_RUN_ALLOWED
#error "v0.9.17 SOURCE-ONLY: electrical driver isolation and break route unvalidated"
#endif
#if MOTOR_ID_BENCH_ALLOWED
#error "v0.9.16 NO MOTOR BENCH: TIM1 BKIN/driver coast not electrically qualified"
#endif''',
'''/* Retain the existing frozen Android v0.3.6 pairing and build guard.
 * This is NOT the original v0.9.14 binary. The artifact filename and
 * sha256, not this compatibility ID, distinguish the experimental RC. */
#define FW_BUILD 0x0914u
#if POWER_STAGE_ARM_ALLOWED || SENSORLESS_RUN_ALLOWED
#error "Motor RC permits E7-only Motor-ID, never drive or sensorless E5"
#endif''')

# Forced disarm remains an irreversible E6/fault/power-off latch from v117.
# The sequencer needs a separate normal output-off period between L pulses:
# maintain passive CH4 ADC trigger and MOE, but disable all six PWM pins.
coast='''/* Motor ID L/R/flux normal pause. This is NOT a terminal emergency.
 * The R/L sequence needs repeated re-arming after a 4ms coast interval.
 * TIM1 CH4 ADC trigger uses MOE on F103; keep it in the passive state.
 * Never invoke on a terminal fault to undo a hardware stop. */
static void motor_id_bench_coast(void) {
    if (g_gate_kill_latched || g_safety_latched) {
        power_stage_force_disarm();
        return;
    }
    g_id_bench_energized_since_ms=0u;
    TIM_DIER(TIM1_BASE)&=~1u;
    motor_id_adc_feedback_off(&g_motor_id_feedback);
    TIM_CCER(TIM1_BASE)=stock_tim1_gate_ccer_off((uint16_t)TIM_CCER(TIM1_BASE));
    gate_pins_to_safe_inputs();
    __asm volatile("dsb" ::: "memory");
    if (g_power_armed) {
        ADC_JSQR=G30_FIXED_JSQR_A;
        ADC2_JSQR=G30_FIXED_JSQR_B;
    }
    g_power_armed=0u;
    g_motor_id_vector_owner=0u;
    g_motor_id_vector.bridge_armed=0u;
    g_motor_id_vector.owning=0u;
    g_motor_id_vector.sector=0u;
    g_motor_pipeline.powered=0u;
    g_motor_pipeline.adc.pending=0u;
    sensorless_control_stop(&ctrl);
    /* If CH4 did not stay active, this is a fault, NOT a pause. */
    if (!tim1_bdtr_guard_passive((uint16_t)TIM_BDTR(TIM1_BASE),
                                 (uint8_t)DEADTIME_DTG,
                                 (uint16_t)TIM_CCER(TIM1_BASE)) ||
        !tim1_bdtr_guard_gpio_down((uint16_t)gate_pin_mode_word_a(),
                                   (uint16_t)gate_pin_mode_word_b(),
                                   (uint16_t)GPIO_ODR(GPIOA_BASE),
                                   (uint16_t)GPIO_ODR(GPIOB_BASE))) {
        g_safety_latched=0xA02Eu;
        power_stage_force_disarm();
    }
}

'''
once('static void motor_hw_gate_off(void *ignored) {',coast+'static void motor_hw_gate_off(void *ignored) {')
once('''static void motor_id_hw_gate_off(void *ignored) {
    (void)ignored;
    power_stage_force_disarm();''',
'''static void motor_id_hw_gate_off(void *ignored) {
    (void)ignored;
    motor_id_bench_coast();''')

# The old v116 source-only runtime assertion rejected every *active* TIM1
# CCER=0x1555 even if a properly guarded E7 had just started.
once('''    if (!tim1_bdtr_guard_passive((uint16_t)TIM_BDTR(TIM1_BASE),
                                 (uint8_t)DEADTIME_DTG,
                                 (uint16_t)ccer)) ok=0u;''',
'''    if (g_power_armed) {
        /* Live E7 only: six gate channels, CH4 trigger, AOE=0,
         * no BKIN/LOCK guesses, exactly the expected deadtime. */
        const uint16_t expected=(uint16_t)(DEADTIME_DTG |
            D_GUARD_OSSI | D_GUARD_OSSR | D_GUARD_MOE);
        if ((uint16_t)TIM_BDTR(TIM1_BASE)!=expected ||
            !stock_tim1_gate_ccer_is_armed((uint16_t)ccer) ||
            !MOTOR_ID_BENCH_ALLOWED || !g_id_bench_requested) ok=0u;
    } else if (!tim1_bdtr_guard_passive((uint16_t)TIM_BDTR(TIM1_BASE),
                                        (uint8_t)DEADTIME_DTG,
                                        (uint16_t)ccer)) ok=0u;''')

# Terminal fault must be tested BEFORE any E7 request. These guards are in
# addition to the g_gate_kill_latched check in motor_hw_arm from v117.
once('''        if(g_id_bench_requested || g_motor_id_vector_owner ||''',
'''        if(g_gate_kill_latched || g_id_bench_requested || g_motor_id_vector_owner ||''')

# Real MCU readback of terminal safety stays in v0.9.17; passive normal pause
# deliberately does not clear MOE, while every pre-existing E6 or fault call
# to power_stage_force_disarm DOES clear it.
assert 'g_gate_kill_latched=1u;' in s
assert 'TIM_BDTR(TIM1_BASE)&=(uint32_t)~D_GUARD_MOE;' in s
assert 'motor_id_bench_coast();' in s
assert 'cmd==0xE6u' in s
assert 'motor_id_auto_tick(&g_motor_id_auto,t0/' in s
assert '#define COMM_CURRENT_SCALE_HW_VALID 0u' in s
assert '#define COMM_ADC_TIMING_HW_VALID 0u' in s
main.write_text(s)
m=make.read_text()
if 'v0_9_17' not in m:raise RuntimeError("Makefile v0.9.17 expected")
make.write_text(m.replace('v0_9_17','v0_9_14_rc'))
print("PASS: E7-only RC source: repeated coast vs terminal MOE=0, Android v0.3.6 protocol retained")
