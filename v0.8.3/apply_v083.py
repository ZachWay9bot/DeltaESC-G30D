#!/usr/bin/env python3
"""Add the stock DRV126 dual-ADC register/acquisition plan to v0.8.2, without activating live sampling."""
from pathlib import Path
import sys
root=Path(sys.argv[1]).resolve(); here=Path(__file__).resolve().parent

def one(s,a,b):
    n=s.count(a)
    if n!=1: raise RuntimeError(f'expected one marker ({n}): {a[:100]!r}')
    return s.replace(a,b,1)

for name in ('stock_dual_adc_plan.c','stock_dual_adc_plan.h'):
    (root/'src'/name).write_bytes((here/'src'/name).read_bytes())

p=root/'Makefile';s=p.read_text()
s=one(s,'src/stock_current_frontend.c','src/stock_current_frontend.c src/stock_dual_adc_plan.c')
s=s.replace('v0_8_2','v0_8_3')
p.write_text(s)

p=root/'src/main.c';s=p.read_text()
s=one(s,'#include "stock_current_frontend.h"','#include "stock_current_frontend.h"\n#include "stock_dual_adc_plan.h"')
s=one(s,'#define FW_BUILD 0x0802u','#define FW_BUILD 0x0803u')
s=one(s,'#define COMM_STOCK_CURRENT_MODEL_VALID 1u','#define COMM_STOCK_CURRENT_MODEL_VALID 1u\n#define COMM_STOCK_DUAL_ADC_PLAN_VALID 1u')
s=s.replace('v0.8.2 stock-current frontend audit must NEVER arm a physical power stage','v0.8.3 stock dual-ADC acquisition audit must NEVER arm a physical power stage')
s=s.replace('v0.8.2 STOCK CURRENT FRONTEND MODEL GATES OFF','v0.8.3 STOCK DUAL ADC ACQUISITION PLAN GATES OFF')
s=s.replace('E1 preflight remains blocked. Stock DRV126 dual-ADC sector/pair/reconstruction model compiled in; current hardware validation bit remains deliberately false.',
'''E1 preflight remains blocked. Stock DRV126 dual-ADC sector/pair/reconstruction model and register plan compiled in; live ISR still uses legacy passive sampler and current hardware validation bit remains deliberately false.''')
# D9 status byte: bit7 current model, bit6 dual-ADC plan. Low action-status bits preserved.
s=one(s,'p[10]=g_phase_map_valid;p[11]=(uint8_t)(g_last_action_status | (COMM_STOCK_CURRENT_MODEL_VALID ? 0x80u : 0u));',
'''p[10]=g_phase_map_valid;p[11]=(uint8_t)(g_last_action_status |
        (COMM_STOCK_CURRENT_MODEL_VALID ? 0x80u : 0u) |
        (COMM_STOCK_DUAL_ADC_PLAN_VALID ? 0x40u : 0u));''')
p.write_text(s)
print('v0.8.3 dual-ADC register plan compiled in; live ADC ISR intentionally unchanged; all hardware blockers remain set')
