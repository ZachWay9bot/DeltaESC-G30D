#!/usr/bin/env python3
"""v0.8.6 overlay on pinned v0.8.5 gate-OFF base (never motor-enabled)."""
from pathlib import Path
import sys,shutil
root=Path(sys.argv[1]).resolve()
here=Path(__file__).resolve().parent
for name in ('stock_adc_quality.h','stock_adc_quality.c'):
    shutil.copyfile(here/'src'/name,root/'src'/name)
def one(s,old,new):
    count=s.count(old)
    if count!=1: raise RuntimeError(f"expected one marker, got {count}: {old[:120]!r}")
    return s.replace(old,new,1)
p=root/'src/main.c'
s=p.read_text()
s=one(s,'#include "stock_dual_adc_runtime.h"',
      '#include "stock_dual_adc_runtime.h"\n#include "stock_adc_quality.h"')
s=one(s,'#define FW_BUILD 0x0805u','#define FW_BUILD 0x0806u')
s=one(s,'v0.8.5 live dual-ADC acquisition must NEVER arm a physical power stage',
      'v0.8.6 ADC integrity and atomic E8 snapshot must NEVER arm a physical power stage')
s=one(s,'static stock_dual_adc_runtime_t g_dual_adc;',
      'static stock_dual_adc_runtime_t g_dual_adc;\nstatic stock_adc_quality_t g_adc_quality;')
s=one(s,'    const uint8_t sector=g_dual_adc.sector; /* channels of just-finished pair */',
'''    const uint8_t sector=g_dual_adc.sector; /* channels of just-finished pair */
    const uint32_t prior=g_last_sample_cycle;
    const uint32_t dt_cycles=g_adc_samples?t0-prior:0u;
    const uint8_t quality=stock_adc_quality_check(&g_adc_quality,sector,s1,s2,
                                 ADC_JSQR,ADC2_JSQR,dt_cycles,PWM_SAMPLE_EXPECT_CYCLES);''')
s=one(s,'    const uint8_t offsets_finished=stock_dual_adc_runtime_capture(&g_dual_adc,s1,s2);',
'''    if (quality & ADCQ_BAD_PAIR) {
        /* Reject invalid or mismapped samples before offset/FOC processing. */
        if (!g_safety_latched) g_safety_latched=0xA004u;
        power_stage_force_disarm();
        stock_dual_adc_runtime_abort_offset(&g_dual_adc);
        g_offset_cal_active=0u;g_offset_cal_valid=0u;
        stock_dual_adc_runtime_next_sector(&g_dual_adc,stock_dual_adc_runtime_rotate(sector));
        const stock_adc_pair_t next_bad=stock_current_pair_for_sector(g_dual_adc.sector);
        ADC_JSQR=stock_current_jsqr_one(next_bad.adc1_channel);
        ADC2_JSQR=stock_current_jsqr_one(next_bad.adc2_channel);
        g_adc_samples++;
        return;
    }
    const uint8_t offsets_finished=stock_dual_adc_runtime_capture(&g_dual_adc,s1,s2);''')
s=one(s,'    case 0xE8u: /* READ-only live ADC1/ADC2 sector/channel/VBUS evidence */\n        put_u16(p,g_last_adc1_pair);',
'''    case 0xE8u: /* READ-only, self-consistent current sample pair. */
        /* Prevent interrupt tearing: E8 retains the v0.3.3 phone ABI. */
        irq_disable();
        put_u16(p,g_last_adc1_pair);''')
s=one(s,'                       (g_vbus_pending?4u:0u));',
'''                       (g_vbus_pending?4u:0u)|
                       (g_adc_quality.fault_latched?8u:0u)|
                       ((g_adc_quality.last_flags&ADCQ_GAP_LONG)?16u:0u)|
                       ((g_adc_quality.last_flags&ADCQ_INTERVAL_SHORT)?32u:0u));''')
s=one(s,'        put_u16(p+14,(uint16_t)(ADC2_JSQR&0xffffu));\n        n=16u;break;',
'''        put_u16(p+14,(uint16_t)(ADC2_JSQR&0xffffu));
        irq_enable();
        n=16u;break;
    case 0xE9u: /* read-only acquisition fault statistics */
        irq_disable();
        put_u32(p,g_adc_quality.total_checked);
        put_u16(p+4,g_adc_quality.invalid_raw_events);
        put_u16(p+6,g_adc_quality.wrong_channel_events);
        put_u16(p+8,g_adc_quality.long_gap_events);
        put_u16(p+10,g_adc_quality.short_gap_events);
        p[12]=g_adc_quality.last_flags;
        p[13]=g_adc_quality.fault_latched;
        put_u16(p+14,(uint16_t)g_safety_latched);
        irq_enable();
        n=16u;break;''')
s=one(s,'    if(ninebot_link_rx_overflows())f|=8u;',
      '    if(g_adc_quality.fault_latched)f|=512u;\n    if(ninebot_link_rx_overflows())f|=8u;')
s=one(s,'        adc_vbus_poll();',
'''        /* Do not wait for the 1s report to recognize a stalled ADC IRQ. */
        if(g_adc_samples && (uint32_t)(g_ms-g_adc_last_ms)>ADC_STALE_MS && !g_safety_latched){
            g_safety_latched=0xA005u;power_stage_force_disarm();
        }
        adc_vbus_poll();''')
assert '#define COMM_CURRENT_SCALE_HW_VALID 0u' in s
assert '#define COMM_GATE_HW_VALID 0u' in s
assert '#define COMM_ADC_TIMING_HW_VALID 0u' in s
assert 'case 0xE8u:' in s and 'case 0xE9u:' in s
p.write_text(s)
p=root/'Makefile'
m=p.read_text()
m=one(m,'src/stock_dual_adc_runtime.c','src/stock_dual_adc_runtime.c src/stock_adc_quality.c')
m=m.replace('v0_8_5','v0_8_6')
assert 'all: safe\n' in m
assert '-DPOWER_STAGE_ARM_ALLOWED=0 -DSENSORLESS_RUN_ALLOWED=0' in m
assert 'active: ' not in m and 'bench: ' not in m
p.write_text(m)
shutil.copyfile(here/'tools/stock_adc_quality_host_test.c',root/'tools/stock_adc_quality_host_test.c')
print("v0.8.6: ADC sample validity; atomic E8, read-only E9, >5ms watchdog; six gates OFF")
