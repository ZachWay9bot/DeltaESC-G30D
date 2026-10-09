#!/usr/bin/env python3
"""Apply v0.8.7 to the pinned v0.8.6 source. Read-only, all gate outputs OFF."""
from pathlib import Path
import shutil,sys
root=Path(sys.argv[1]).resolve()
here=Path(__file__).resolve().parent
for name in ('stock_adc_snapshot.c','stock_adc_snapshot.h'):
    shutil.copyfile(here/'src'/name,root/'src'/name)
shutil.copyfile(here/'tools/stock_adc_snapshot_host_test.c',root/'tools/stock_adc_snapshot_host_test.c')
p=root/'src/main.c'
s=p.read_text()
def one(old,new):
    global s
    count=s.count(old)
    if count!=1:raise RuntimeError("Expected unique source marker, got "+str(count)+": "+repr(old[:100]))
    s=s.replace(old,new,1)
one('#include "stock_adc_quality.h"',
    '#include "stock_adc_quality.h"\n#include "stock_adc_snapshot.h"')
one('#define FW_BUILD 0x0806u','#define FW_BUILD 0x0807u')
one('#error "v0.8.6 ADC integrity and atomic E8 snapshot must NEVER arm a physical power stage"',
    '#error "v0.8.7 paired ADC evidence must NEVER arm a physical power stage"')
one('static stock_adc_quality_t g_adc_quality;',
    'static stock_adc_quality_t g_adc_quality;\nstatic stock_adc_snapshot_t g_pair_snapshot;')
one('    if(!(ADC_SR&ADC_SR_JEOC))return;\n    const uint16_t s1=(uint16_t)ADC_JDR1;',
    '''    const uint32_t adc1_sr=ADC_SR,adc2_sr=ADC2_SR;
    if(!(adc1_sr&ADC_SR_JEOC))return;
    const uint32_t jsqr1=ADC_JSQR,jsqr2=ADC2_JSQR;
    const uint16_t s1=(uint16_t)ADC_JDR1;''')
one('    const uint8_t sector=g_dual_adc.sector; /* channels of just-finished pair */',
    '''    const uint8_t sector=g_dual_adc.sector; /* channels of just-finished pair */
    stock_adc_snapshot_capture(&g_pair_snapshot,sector,s1,s2,jsqr1,jsqr2,
                               (uint16_t)adc1_sr,(uint16_t)adc2_sr,g_ms);''')
one('stock_adc_quality_check(&g_adc_quality,sector,s1,s2,\n                                 ADC_JSQR,ADC2_JSQR,dt_cycles,PWM_SAMPLE_EXPECT_CYCLES)',
    'stock_adc_quality_check(&g_adc_quality,sector,s1,s2,\n                                 jsqr1,jsqr2,dt_cycles,PWM_SAMPLE_EXPECT_CYCLES)')
one('put_u16(p+14,(uint16_t)(ADC2_JSQR&0xffffu));',
    'put_u16(p+14,(uint16_t)(g_pair_snapshot.adc2_jsqr&0xffffu));')
one('        n=16u;break;\n    case 0xE9u: /* passive ADC integrity counters, still no calibrated units */',
    '''        n=16u;break;
    case 0xEAu: /* full JSQR and JEOC state of one completed ADC sample pair */
        irq_disable();
        stock_adc_snapshot_encode_ea(&g_pair_snapshot,p);
        irq_enable();
        n=16u;break;
    case 0xE9u: /* passive ADC integrity counters, still no calibrated units */''')
one('uart_puts("DeltaESC G30D v0.8.5 LIVE DUAL ADC INJECTED + REGULAR VBUS GATES OFF. GATES PERMANENTLY OFF.\\r\\n");',
    'uart_puts("DeltaESC G30D v0.8.7 PAIRED ADC EVIDENCE + E8/E9/EA DIAG. GATES OFF.\\r\\n");')
p.write_text(s)
p=root/'Makefile'
m=p.read_text()
assert m.count('src/stock_adc_quality.c')==1
m=m.replace('src/stock_adc_quality.c','src/stock_adc_quality.c src/stock_adc_snapshot.c')
assert 'v0_8_6' in m
m=m.replace('v0_8_6','v0_8_7')
assert 'all: safe\n' in m and 'active: ' not in m and 'bench: ' not in m
assert '-DPOWER_STAGE_ARM_ALLOWED=0 -DSENSORLESS_RUN_ALLOWED=0' in m
p.write_text(m)
assert '#define COMM_ADC_TIMING_HW_VALID 0u' in s
assert '#define COMM_CURRENT_SCALE_HW_VALID 0u' in s
assert '#define COMM_GATE_HW_VALID 0u' in s
assert '#if POWER_STAGE_ARM_ALLOWED || SENSORLESS_RUN_ALLOWED' in s
print("PASS v0.8.7 overlay: sample record before sector advance, consistent E8, EA raw evidence, gates OFF")
