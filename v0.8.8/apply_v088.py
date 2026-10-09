#!/usr/bin/env python3
"""Apply v0.8.8 on pinned v0.8.7 source. SOURCE ONLY. Gate outputs OFF."""
from pathlib import Path
import sys,shutil
root=Path(sys.argv[1]).resolve()
here=Path(__file__).resolve().parent
for name in ('stock_adc_timing.h','stock_adc_timing.c'):
    target=root/'src'/name
    if not target.is_file():shutil.copyfile(here/'src'/name,target)
def one(text,old,new):
    count=text.count(old)
    if count!=1:raise RuntimeError(f'expected one marker, got {count}: {old[:110]!r}')
    return text.replace(old,new,1)
p=root/'src/main.c';s=p.read_text()
s=one(s,'#include "stock_adc_snapshot.h"',
      '#include "stock_adc_snapshot.h"\n#include "stock_adc_timing.h"')
s=one(s,'#define FW_BUILD 0x0807u','#define FW_BUILD 0x0808u')
s=one(s,'#error "v0.8.7 paired ADC evidence must NEVER arm a physical power stage"',
      '#error "v0.8.8 TIM1+ADC timing evidence must NEVER arm a physical power stage"')
s=one(s,'static stock_adc_snapshot_t g_pair_snapshot;',
      'static stock_adc_snapshot_t g_pair_snapshot;\nstatic stock_adc_timing_t g_timer_evidence;')
s=one(s,'    const uint32_t jsqr1=ADC_JSQR,jsqr2=ADC2_JSQR;',
'''    /* TIM1 register snapshot at ADC IRQ entry, AFTER conversion and ISR
     * latency. This does not timestamp the analog sample. */
    stock_adc_timing_capture(&g_timer_evidence,
        (uint16_t)TIM_CNT(TIM1_BASE),(uint16_t)TIM_CCR4(TIM1_BASE),
        (uint16_t)TIM_ARR(TIM1_BASE),(uint16_t)TIM_CR1(TIM1_BASE),
        (uint16_t)TIM_CCER(TIM1_BASE),(uint16_t)TIM_BDTR(TIM1_BASE),
        (uint16_t)TIM_SR(TIM1_BASE),t0);
    const uint32_t jsqr1=ADC_JSQR,jsqr2=ADC2_JSQR;''')
s=one(s,'    case 0xEAu: /* full JSQR and JEOC state of one completed ADC sample pair */',
'''    case 0xEBu: /* register evidence, not physical sample time */
        irq_disable();
        stock_adc_timing_encode_eb(&g_timer_evidence,p);
        irq_enable();
        n=16u;break;
    case 0xECu: /* IRQ interval and timer config checks (diagnostic only) */
        irq_disable();
        stock_adc_timing_encode_ec(&g_timer_evidence,p);
        irq_enable();
        n=16u;break;
    case 0xEAu: /* full JSQR and JEOC state of one completed ADC sample pair */''')
s=one(s,'DeltaESC G30D v0.8.7 PAIRED ADC EVIDENCE + E8/E9/EA DIAG. GATES OFF.',
      'DeltaESC G30D v0.8.8 ADC/TIM1 IRQ EVIDENCE E8-EC. GATES OFF.')
assert '#define COMM_GATE_HW_VALID 0u' in s
assert '#define COMM_ADC_TIMING_HW_VALID 0u' in s
assert '#define COMM_CURRENT_SCALE_HW_VALID 0u' in s
assert '#if POWER_STAGE_ARM_ALLOWED || SENSORLESS_RUN_ALLOWED' in s
p.write_text(s)
p=root/'Makefile';m=p.read_text()
m=one(m,'src/stock_adc_snapshot.c','src/stock_adc_snapshot.c src/stock_adc_timing.c')
m=m.replace('v0_8_7','v0_8_8')
assert 'all: safe\n' in m and 'active: ' not in m and 'bench: ' not in m
assert '-DPOWER_STAGE_ARM_ALLOWED=0 -DSENSORLESS_RUN_ALLOWED=0' in m
p.write_text(m)
shutil.copyfile(here/'tools/stock_adc_timing_host_test.c',root/'tools/stock_adc_timing_host_test.c')
print('PASS v0.8.8 TIM1 IRQ-entry EB/EC evidence, unchanged BLE, gates OFF')
