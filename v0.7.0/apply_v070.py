#!/usr/bin/env python3
"""Rebuild exact v0.7.0 source from v0.6.10. Fail on missing markers."""
from pathlib import Path
import sys
root=Path(sys.argv[1])
def replace_once(s,a,b):
    if s.count(a)!=1:raise RuntimeError(f'expected 1 occurrence of {a!r}, got {s.count(a)}')
    return s.replace(a,b,1)
p=root/'src/main.c';s=p.read_text()
s=replace_once(s,'#include "power_logic.h"','#include "power_logic.h"\n#include "motor_probe.h"')
s=replace_once(s,'#define FW_BUILD 0x0609u','''#define FW_BUILD 0x0700u
#if POWER_STAGE_ARM_ALLOWED || SENSORLESS_RUN_ALLOWED
#error "v0.7.0 motor probe must NEVER arm a physical power stage"
#endif''')
s=replace_once(s,'    g_adc_samples++;if((g_adc_samples&0x3Fu)==0u)(void)gate_safety_check();','''    motor_probe_capture(s0,s1,s2,s3,g_adc_offset,g_ms,
                        g_sample_interval_min,g_sample_interval_max,
                        g_max_control_cycles,g_power_armed,
                        ADC_JSQR,ADC_CR2,TIM_CCER(TIM1_BASE),TIM_BDTR(TIM1_BASE));
    g_adc_samples++;if((g_adc_samples&0x3Fu)==0u)(void)gate_safety_check();''')
s=replace_once(s,'sensorless_control_init(&ctrl);power_button_init(','sensorless_control_init(&ctrl);motor_probe_init();power_button_init(')
s=replace_once(s,'uart_puts("DeltaESC G30D v0.6.7 v0.1.x-app-compat + staged-IAP firmware\\r\\n");','uart_puts("DeltaESC G30D v0.7.0 ST-LINK PASSIVE MOTOR ADC PROBE. GATES PERMANENTLY OFF.\\r\\n");')
p.write_text(s)
p=root/'Makefile';s=p.read_text()
s=replace_once(s,'src/power_logic.c','src/power_logic.c src/motor_probe.c')
s=replace_once(s,'all: safe active bench','all: safe')
s=replace_once(s,'active: $(ACTIVE_BIN)\nbench: $(BENCH_BIN)\n','')
a=s.index('$(ACTIVE_ELF):');b=s.index('clean: ; rm -rf build')
s=s[:a]+s[b:]
s=s.replace('v0_6_7','v0_7_0').replace('v0.6.7','v0.7.0')
s=replace_once(s,'CFLAGS_COMMON = --target','CFLAGS_COMMON = -g3 --target')
p.write_text(s)
for name in ('motor_probe.c','motor_probe.h'):
    (root/'src'/name).write_text((Path(__file__).resolve().parent/'src'/name).read_text())
print('v0.7.0 passive ADC probe installed; motor gates compile-disabled')
