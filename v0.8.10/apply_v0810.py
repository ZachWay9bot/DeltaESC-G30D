#!/usr/bin/env python3
"""v0.8.10: fix STM32F103 ADC DUALMOD 0001 -> 0101, require ADC2 JEOC.
SOURCE ONLY, GATES OFF. Apply once on frozen v0.8.9 source.
"""
from pathlib import Path
import sys,shutil
root=Path(sys.argv[1]).resolve();here=Path(__file__).resolve().parent
def one(s,a,b):
    n=s.count(a)
    if n!=1:raise RuntimeError(f'v0810 patch expected 1 anchor, got {n}: {a[:110]!r}')
    return s.replace(a,b,1)
for name in ('stock_adc_mode_guard.h','stock_adc_mode_guard.c'):
    shutil.copyfile(here/'src'/name,root/'src'/name)
shutil.copyfile(here/'tools/test_stock_adc_mode_guard.c',root/'tools/test_stock_adc_mode_guard.c')
p=root/'src/stock_dual_adc_plan.h';s=p.read_text()
s=one(s,'STOCK_ADC_MODE_REG_INJEC_SIMULT = 0x00010000u',
      'STOCK_ADC_MODE_REG_INJEC_SIMULT = 0x00050000u')
p.write_text(s)
p=root/'src/main.c';s=p.read_text()
s=one(s,'#include "stock_adc_correlated.h"',
      '#include "stock_adc_correlated.h"\n#include "stock_adc_mode_guard.h"')
s=one(s,'#define FW_BUILD 0x0809u','#define FW_BUILD 0x080Au')
s=one(s,'#error "v0.8.9 atomic ED evidence must NEVER arm a physical power stage"',
      '#error "v0.8.10 corrected ADC DUALMOD and missing ADC2 JEOC must NEVER arm a physical power stage"')
s=one(s,'static stock_adc_correlated_t g_correlated_evidence;',
      'static stock_adc_correlated_t g_correlated_evidence;\nstatic g30_adc_mode_guard_t g_adc_mode_guard;')
s=one(s,'    if(!(adc1_sr&ADC_SR_JEOC))return;',
'''    if(!(adc1_sr&ADC_SR_JEOC))return;
    /* RM0008 DUALMOD=0101 (injected-only simultaneous). DUALMOD=0001
     * accidentally coupled regular ADC1 PA1 VBUS to ADC2 in v0.8.9.
     * A missing ADC2 JEOC MUST NOT deliver a stale ADC2_JDR1 to FOC. */
    const uint8_t adc_mode_fault=g30_adc_mode_capture(&g_adc_mode_guard,
                                       ADC_CR1,ADC2_CR1,ADC_CR2,adc2_sr);''')
s=one(s,'    const uint16_t s2=(uint16_t)ADC2_JDR1;',
'''    /* Mark incomplete ADC2 conversion with an invalid 12-bit sentinel.
     * Never use a previous ADC2_JDR1 reading as today's current. */
    const uint16_t s2=(adc_mode_fault&G30_ADCMODE_ADC2_NOT_READY)?
                      0xffffu:(uint16_t)ADC2_JDR1;''')
s=one(s,'&g_pair_snapshot,&g_timer_evidence,quality);',
      '&g_pair_snapshot,&g_timer_evidence,(uint8_t)(quality|(adc_mode_fault?ADCQ_RAW_INVALID:0u)));')
s=one(s,'    if (quality & ADCQ_BAD_PAIR) {',
      '    if ((quality & ADCQ_BAD_PAIR) || adc_mode_fault) {')
s=one(s,'        if (!g_safety_latched) g_safety_latched=0xA004u;',
'''        if(!g_safety_latched)g_safety_latched=adc_mode_fault?
            ((adc_mode_fault&G30_ADCMODE_ADC2_NOT_READY)?0xA007u:0xA008u):0xA004u;''')
s=one(s,'    case 0xEDu: /* one 16-byte READ-ONLY ADC+TIM1 sample, versioned ABI */',
'''    case 0xEEu: /* read-only ADC operating mode and ADC2 JEOC qualification */
        irq_disable();
        g30_adc_mode_encode_ee(&g_adc_mode_guard,p);
        irq_enable();
        n=16u;break;
    case 0xEDu: /* one 16-byte READ-ONLY ADC+TIM1 sample, versioned ABI */''')
s=one(s,'DeltaESC G30D v0.8.9 CORRELATED ADC/TIM1 ED DIAG. GATES OFF.',
      'DeltaESC G30D v0.8.10 ADC DUALMOD 0101 + ADC2 JEOC GUARD EE. GATES OFF.')
for sentinel in ('#define COMM_CURRENT_SCALE_HW_VALID 0u',
                 '#define COMM_GATE_HW_VALID 0u',
                 '#define COMM_ADC_TIMING_HW_VALID 0u',
                 '#if POWER_STAGE_ARM_ALLOWED || SENSORLESS_RUN_ALLOWED',
                 'case 0xEEu:',
                 'adc_mode_fault&G30_ADCMODE_ADC2_NOT_READY'):
    assert sentinel in s,sentinel
p.write_text(s)
p=root/'Makefile';m=p.read_text()
m=one(m,'src/stock_adc_correlated.c','src/stock_adc_correlated.c src/stock_adc_mode_guard.c')
m=m.replace('v0_8_9','v0_8_10')
assert 'all: safe\n' in m and 'active: ' not in m and 'bench: ' not in m
assert '-DPOWER_STAGE_ARM_ALLOWED=0 -DSENSORLESS_RUN_ALLOWED=0' in m
p.write_text(m)
print('PASS v0.8.10 patch: ADC injected-only DUALMOD 0101, ADC2 JEOC fail-closed, 16B EE, gates OFF')
