#!/usr/bin/env python3
"""Apply the stock DRV126 current-frontend model to a reconstructed v0.8.1 tree."""
from pathlib import Path
import sys
root=Path(sys.argv[1]).resolve()
repo=Path(__file__).resolve().parent

def one(s,old,new):
    n=s.count(old)
    if n!=1: raise RuntimeError(f'expected one marker ({n}): {old[:100]!r}')
    return s.replace(old,new,1)

for name in ('stock_current_frontend.c','stock_current_frontend.h'):
    (root/'src'/name).write_bytes((repo/'src'/name).read_bytes())

p=root/'Makefile'; s=p.read_text()
s=one(s,'src/commissioning_guard.c','src/commissioning_guard.c src/stock_current_frontend.c')
s=s.replace('v0_8_1','v0_8_2')
p.write_text(s)

p=root/'src/main.c'; s=p.read_text()
s=one(s,'#include "commissioning_guard.h"','#include "commissioning_guard.h"\n#include "stock_current_frontend.h"')
s=one(s,'#define FW_BUILD 0x0801u','#define FW_BUILD 0x0802u')
s=one(s,'#define COMM_ADC_TIMING_HW_VALID 0u','#define COMM_ADC_TIMING_HW_VALID 0u\n#define COMM_STOCK_CURRENT_MODEL_VALID 1u')
s=one(s,'p[10]=g_phase_map_valid;p[11]=g_last_action_status;','p[10]=g_phase_map_valid;p[11]=(uint8_t)(g_last_action_status | (COMM_STOCK_CURRENT_MODEL_VALID ? 0x80u : 0u));')
s=s.replace('v0.8.1 BLE commissioning audit must NEVER arm a physical power stage','v0.8.2 stock-current frontend audit must NEVER arm a physical power stage')
s=s.replace('v0.8.1 DASHBLE COMMISSIONING GUARD GATES OFF','v0.8.2 STOCK CURRENT FRONTEND MODEL GATES OFF')
s=s.replace('E1 = read-only commissioning preflight ACK; E2-E4 remain locked. D9 bytes 6..15 expose exact guard blockers.','E1 preflight remains blocked. Stock DRV126 dual-ADC sector/pair/reconstruction model compiled in; current hardware validation bit remains deliberately false.')
p.write_text(s)
print('v0.8.2 stock current frontend model applied; hardware current-validation blocker remains set')
