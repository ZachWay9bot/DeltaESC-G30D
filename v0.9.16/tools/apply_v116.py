#!/usr/bin/env python3
"""Deterministic, fail-on-drift safe-mode overlay on reconstructed v0.9.14.

SOURCE ONLY: no motor-ID bench, no physical gate-enable permission.
Preserves the validated NinebotCrypto/UART transport unchanged.
"""
import pathlib
import shutil
import sys

root=pathlib.Path(__file__).resolve().parents[2]
target=pathlib.Path(sys.argv[1]).resolve()
p=target/"src/main.c"
m=target/"Makefile"

def one(s,old,new):
    count=s.count(old)
    if count!=1:
        raise RuntimeError(f"v0.9.16 anchor drift: expected exactly one {old!r}, got {count}")
    return s.replace(old,new,1)

source=p.read_text()
source=one(source,'#include "stock_tim1_gate.h"',
    '#include "stock_tim1_gate.h"\n#include "tim1_bdtr_guard.h"')
source=one(source,'#define FW_BUILD 0x0914u',
    '#define FW_BUILD 0x0916u\n'
    '#if MOTOR_ID_BENCH_ALLOWED\n'
    '#error "v0.9.16 NO MOTOR BENCH: TIM1 BKIN/driver coast not electrically qualified"\n'
    '#endif')
# Grounded on STM32 RM0008: AOE=1 re-enables MOE at next UEV after break.
# LOCK=1 freezes AOE/BKE/BKP. No guess at BKE/BKP until AMP_LIM is proven.
# Do NOT remove MOE from the ADC-diagnostic CH4-only state: the STM32F1
# TIM1-CC4 injected trigger may depend on MOE being asserted.
original=('    TIM_CCER(TIM1_BASE)=stock_tim1_gate_ccer_off(STOCK_TIM1_GATE_RUN_CCER); '
          'TIM_BDTR(TIM1_BASE)=DEADTIME_DTG|TIM_BDTR_OSSI|TIM_BDTR_OSSR|TIM_BDTR_MOE;')
replacement='''    TIM_CCER(TIM1_BASE)=stock_tim1_gate_ccer_off(STOCK_TIM1_GATE_RUN_CCER);
    /* A prior LOCK=1 is immutable until MCU reset. Never blindly continue. */
    if (!tim1_bdtr_guard_preinit((uint16_t)TIM_BDTR(TIM1_BASE))) {
        g_safety_latched=0xA02Au;
        gate_pins_to_safe_inputs();
        return;
    }
    /* Passive ADC-only profile: AOE=0, BKE=0, BKP=0, LOCK=0,
       DTG unchanged and CH1/2/3 + CHxN disabled via CCER. */
    TIM_BDTR(TIM1_BASE)=DEADTIME_DTG|TIM_BDTR_OSSI|TIM_BDTR_OSSR|TIM_BDTR_MOE;
    if (!tim1_bdtr_guard_passive((uint16_t)TIM_BDTR(TIM1_BASE),
                                 (uint8_t)DEADTIME_DTG,
                                 (uint16_t)TIM_CCER(TIM1_BASE))) {
        g_safety_latched=0xA02Bu;
        TIM_CCER(TIM1_BASE)=stock_tim1_gate_ccer_off(
                             (uint16_t)TIM_CCER(TIM1_BASE));
        gate_pins_to_safe_inputs();
        return;
    }'''
source=one(source,original,replacement)
source=one(source,'    if (!(TIM_BDTR(TIM1_BASE)&TIM_BDTR_MOE)) ok=0u;',
'''    if (!(TIM_BDTR(TIM1_BASE)&TIM_BDTR_MOE)) ok=0u;
    /* Runtime assertion: any AOE/BKE/BKP/LOCK change or unexpected
       CH1..3 / complementary enable latches a software safety fault. */
    if (!tim1_bdtr_guard_passive((uint16_t)TIM_BDTR(TIM1_BASE),
                                 (uint8_t)DEADTIME_DTG,
                                 (uint16_t)ccer)) ok=0u;''')
p.write_text(source)
make=m.read_text()
if "v0_9_14" not in make:
    raise RuntimeError("v0.9.14 Makefile not found")
m.write_text(make.replace("v0_9_14","v0_9_16"))
dest=target/"src/tim1_bdtr_guard.h"
if dest.exists():
    raise RuntimeError("Unexpected existing BDTR header")
shutil.copyfile(root/"v0.9.16/src/tim1_bdtr_guard.h",dest)
assert "MOTOR_ID_BENCH_ALLOWED" in source
assert source.count("tim1_bdtr_guard_passive(")==2
assert source.count("tim1_bdtr_guard_preinit(")==1
print("PASS: v0.9.16 FAIL-CLOSED source overlay (AOE=0, no BKE guess, no live motor build)")
