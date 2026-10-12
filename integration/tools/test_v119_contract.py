#!/usr/bin/env python3
"""Fail-closed source audit for combined Motor-RC and FA/FB read-only telemetry.

Static source assertions are not hardware electrical qualification.
"""
from pathlib import Path
import sys
p=Path(sys.argv[1])/'src/main.c'
src=p.read_text()
make=(p.parent.parent/'Makefile').read_text()
def require(found, msg):
    if not found:
        raise SystemExit('FAIL: '+msg)
require(src.count('#define FW_BUILD 0x0919u')==1,'unique 0x0919 identity')
require('v0_9_19' in make,'distinct binary names')
require(src.count('case 0xFAu: case 0xFBu:')==1,'one read-only FA/FB handler')
require(src.count('g30_adc_proof_eval(&x)')==1,'ADC proof attached to live state')
require('g30_adc_proof_page_fa(p,&x,o,fault)' in src,'FA packed reply')
require('g30_adc_proof_page_fb(p,&x,o,bdtr,vbus_raw,ilast,ipeak)' in src,'FB packed reply')
require('if(!(primask_before&1u))irq_enable();' in src,'restore PRIMASK accurately')
require('#error "Motor RC permits E7-only Motor-ID, never drive or sensorless E5"' in src,'E5/drive compile denial')
require('g_gate_kill_latched=1u;' in src,'terminal gate kill latch')
require('TIM_BDTR(TIM1_BASE)&=(uint32_t)~D_GUARD_MOE;' in src,'terminal MOE disarm')
require('motor_id_bench_coast();' in src,'normal coast separate from terminal stop')
require('g_gate_kill_latched || g_id_bench_requested' in src,'E7 cannot override terminal gate kill')
require('#define COMM_CURRENT_SCALE_HW_VALID 0u' in src,'unqualified physical gain remains false')
require('#define COMM_ADC_TIMING_HW_VALID 0u' in src,'unqualified ADC timing remains false')
require('case 0xE6u:' in src or 'cmd==0xE6u' in src,'explicit emergency E6')
print('PASS: 0x0919 RC + read-only FA/FB, E5 denied, E6/MOE latch preserved')
