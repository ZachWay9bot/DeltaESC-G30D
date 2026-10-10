#!/usr/bin/env python3
"""v0.9.17 EBiCS-style terminal emergency MOE kill overlay onto v0.9.16.
Requires exact versioned anchors. No motor firmware or external hardware.
"""
from pathlib import Path
import shutil,sys

root=Path(__file__).resolve().parents[2]
target=Path(sys.argv[1]).resolve()
p=target/'src/main.c'
make=target/'Makefile'

def one(s,old,new):
    n=s.count(old)
    if n!=1:raise RuntimeError('v0.9.17 source drift: '+repr(old[:140])+' count='+str(n))
    return s.replace(old,new,1)

s=p.read_text()
s=one(s,'#include "tim1_bdtr_guard.h"',
      '#include "tim1_bdtr_guard.h"\n#include "tim1_emergency_stop.h"')
s=one(s,'#define FW_BUILD 0x0916u',
'''#define FW_BUILD 0x0917u
#if MOTOR_ID_BENCH_ALLOWED || POWER_STAGE_ARM_ALLOWED || SENSORLESS_RUN_ALLOWED
#error "v0.9.17 SOURCE-ONLY: electrical driver isolation and break route unvalidated"
#endif''')
s=one(s,'static volatile uint8_t g_id_bench_requested;',
'''static volatile uint8_t g_id_bench_requested;
/* Once STOP/E6/fault requests the hardware kill, timer rearm is prohibited
 * until a full MCU reset. Do not reset this latch via BLE or diagnostics. */
static volatile uint8_t g_gate_kill_latched;''')

s=one(s,'static void power_stage_force_disarm(void) {',
'''static void power_stage_force_disarm(void) {
    /* EBiCS motor_disable_pwm() principle. Every forced STOP is terminal:
     * loss of ADC-CH4 injected trigger is acceptable on a latched fault.
     * This flag is written BEFORE any timer/GPIO accesses. */
    g_gate_kill_latched=1u;''')
s=one(s,
'''    TIM_CCER(TIM1_BASE)=stock_tim1_gate_ccer_off((uint16_t)TIM_CCER(TIM1_BASE));
    gate_pins_to_safe_inputs();''',
'''    /* First disconnect all three complementary TIM1 pairs, then remove
     * MOE. Unlike passive CH4 measurement this is a terminal hardware kill.
     * AOE=0 must already have been established by v0.9.16 initialization.
     * Never write AOE/BKP/BKE/LOCK based on an unvalidated AMP_LIM pin. */
    TIM_CCER(TIM1_BASE)=stock_tim1_gate_ccer_off((uint16_t)TIM_CCER(TIM1_BASE));
    TIM_BDTR(TIM1_BASE)&=(uint32_t)~D_GUARD_MOE;
    gate_pins_to_safe_inputs();
    if (!tim1_emergency_registers_off((uint16_t)TIM_BDTR(TIM1_BASE),
                                       (uint16_t)TIM_CCER(TIM1_BASE)) ||
        !tim1_bdtr_guard_gpio_down((uint16_t)gate_pin_mode_word_a(),
                                   (uint16_t)gate_pin_mode_word_b(),
                                   (uint16_t)GPIO_ODR(GPIOA_BASE),
                                   (uint16_t)GPIO_ODR(GPIOB_BASE))) {
        g_safety_latched=0xA02Cu; /* failed emergency hardware readback */
    }''')

# Any state-machine or watchdog test after the kill must expect MOE=0,
# and must not re-enable it just to recover passive ADC diagnostics.
s=one(s,'static uint32_t gate_safety_check(void) {',
'''static uint32_t gate_safety_check(void) {
    if (g_gate_kill_latched) {
        const uint8_t confirmed=(uint8_t)(
            tim1_emergency_registers_off((uint16_t)TIM_BDTR(TIM1_BASE),
                                          (uint16_t)TIM_CCER(TIM1_BASE)) &&
            tim1_bdtr_guard_gpio_down((uint16_t)gate_pin_mode_word_a(),
                                      (uint16_t)gate_pin_mode_word_b(),
                                      (uint16_t)GPIO_ODR(GPIOA_BASE),
                                      (uint16_t)GPIO_ODR(GPIOB_BASE)));
        if (!confirmed) {
            g_safety_latched=0xA02Du;
            /* Idempotent hardware kill. Do not recursively call ourselves. */
            TIM_CCER(TIM1_BASE)=stock_tim1_gate_ccer_off(
                                 (uint16_t)TIM_CCER(TIM1_BASE));
            TIM_BDTR(TIM1_BASE)&=(uint32_t)~D_GUARD_MOE;
            gate_pins_to_safe_inputs();
        }
        return confirmed;
    }''')

# Defend subsequent regressions: never build a motor path that allows a
# terminal fault latch to be ignored; source-only image also compile-bans all
# potentially powered flags.
s=one(s, 'static uint8_t motor_hw_arm(void *ignored) {',
'''static uint8_t motor_hw_arm(void *ignored) {
    if (g_gate_kill_latched) return 0u;''')
s=one(s, 'static void tim1_pwm_and_adc_trigger_init(void) {',
'''static void tim1_pwm_and_adc_trigger_init(void) {
    if (g_gate_kill_latched) return; /* never resurrect a killed MOE */''')

# The existing pre-init test catches stock AOE/LOCK snapshot and sets a
# latch, but must also physically remove MOE on rejection.
s=one(s,
'''        g_safety_latched=0xA02Au;
        gate_pins_to_safe_inputs();
        return;''',
'''        g_safety_latched=0xA02Au;
        power_stage_force_disarm();
        return;''')
s=one(s,
'''        g_safety_latched=0xA02Bu;
        TIM_CCER(TIM1_BASE)=stock_tim1_gate_ccer_off(
                             (uint16_t)TIM_CCER(TIM1_BASE));
        gate_pins_to_safe_inputs();
        return;''',
'''        g_safety_latched=0xA02Bu;
        power_stage_force_disarm();
        return;''')

# Ensure no ordinary stop can silently re-enable the passive ADC trigger.
# Every forced shutdown is terminal, not automatically self-clearing.
s=one(s, '    if (c1>PWM_ARR || c2>PWM_ARR || c3>PWM_ARR ||',
'''    if (g_gate_kill_latched) return 0u;
    if (c1>PWM_ARR || c2>PWM_ARR || c3>PWM_ARR ||''')

assert s.count('g_gate_kill_latched=1u;')==1
assert s.count('tim1_emergency_registers_off(')==2
assert 'MOTOR_ID_BENCH_ALLOWED || POWER_STAGE_ARM_ALLOWED || SENSORLESS_RUN_ALLOWED' in s
p.write_text(s)
old=make.read_text()
if 'v0_9_16' not in old:raise RuntimeError('v0.9.16 Makefile expected')
make.write_text(old.replace('v0_9_16','v0_9_17'))
dest=target/'src/tim1_emergency_stop.h'
if dest.exists():raise RuntimeError('Unexpected preexisting emergency header')
shutil.copyfile(root/'v0.9.17/src/tim1_emergency_stop.h',dest)
print('PASS v0.9.17 source overlay: latched MOE=0, no E7/drive build, readback guarded')
