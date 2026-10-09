#!/usr/bin/env python3
"""v0.8.9 single-frame ED ADC1/ADC2/TIM1 evidence, always motor GATES OFF."""
from pathlib import Path
import sys,shutil
root=Path(sys.argv[1]).resolve();here=Path(__file__).resolve().parent
def one(s,old,new):
    if s.count(old)!=1:raise RuntimeError(f"expected one anchor, got {s.count(old)}: {old[:115]}")
    return s.replace(old,new,1)
for name in ("stock_adc_correlated.c","stock_adc_correlated.h"):
    shutil.copyfile(here/"src"/name,root/"src"/name)
shutil.copyfile(here/"tools/test_stock_adc_correlated.c",root/"tools/test_stock_adc_correlated.c")
p=root/"src/main.c";s=p.read_text()
s=one(s,'#include "stock_adc_timing.h"',
       '#include "stock_adc_timing.h"\n#include "stock_adc_correlated.h"')
s=one(s,'#define FW_BUILD 0x0808u','#define FW_BUILD 0x0809u')
s=one(s,'v0.8.8 TIM1+ADC timing evidence must NEVER arm a physical power stage',
       'v0.8.9 atomic ED evidence must NEVER arm a physical power stage')
s=one(s,'static stock_adc_timing_t g_timer_evidence;',
       'static stock_adc_timing_t g_timer_evidence;\nstatic stock_adc_correlated_t g_correlated_evidence;')
s=one(s,'    g_last_adc1_pair=s1;g_last_adc2_pair=s2;g_last_pair_sector=sector;',
'''    /* Completed pair and timer refer to precisely this ISR. This must
     * happen BEFORE programming the next pair of injected channels. */
    stock_adc_correlated_capture(&g_correlated_evidence,
                                 &g_pair_snapshot,&g_timer_evidence,quality);
    g_last_adc1_pair=s1;g_last_adc2_pair=s2;g_last_pair_sector=sector;''')
s=one(s,'    case 0xECu: /* IRQ interval and timer config checks (diagnostic only) */',
'''    case 0xEDu: /* one 16-byte READ-ONLY ADC+TIM1 sample, versioned ABI */
        irq_disable();
        stock_adc_correlated_encode_ed(&g_correlated_evidence,p);
        irq_enable();
        n=16u;break;
    case 0xECu: /* IRQ interval and timer config checks (diagnostic only) */''')
s=one(s,'DeltaESC G30D v0.8.8 ADC/TIM1 IRQ EVIDENCE E8-EC. GATES OFF.',
       'DeltaESC G30D v0.8.9 CORRELATED ADC/TIM1 ED DIAG. GATES OFF.')
for marker in ('#define COMM_GATE_HW_VALID 0u',
               '#define COMM_ADC_TIMING_HW_VALID 0u',
               '#define COMM_CURRENT_SCALE_HW_VALID 0u',
               '#if POWER_STAGE_ARM_ALLOWED || SENSORLESS_RUN_ALLOWED',
               'case 0xEDu:', 'stock_adc_correlated_capture(&g_correlated_evidence'):
    assert marker in s,marker
p.write_text(s)
p=root/"Makefile";m=p.read_text()
m=one(m,'src/stock_adc_timing.c','src/stock_adc_timing.c src/stock_adc_correlated.c')
m=m.replace('v0_8_8','v0_8_9')
assert 'all: safe\n' in m and 'active: ' not in m and 'bench: ' not in m
assert '-DPOWER_STAGE_ARM_ALLOWED=0 -DSENSORLESS_RUN_ALLOWED=0' in m
p.write_text(m)
print("PASS v0.8.9 correlated single-ISR E8+EB evidence in read-only ED, no new writes, gates OFF")
