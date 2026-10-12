#!/usr/bin/env python3
"""DeltaESC v0.9.19: add v0.9.18 ADC proof to Motor-RC E7 without changing actuation.

Does NOT substitute synthetic ADC data or authorize flashing.
Keep the existing experimental E7-only gate/kill guards untouched.
Use a distinct build identity to prevent old Android 0x0914 commands.
"""
from pathlib import Path
import shutil,sys

root=Path(__file__).resolve().parents[2]
target=Path(sys.argv[1]).resolve()
p=target/'src/main.c'; m=target/'Makefile'

def one(s,old,new):
    hits=s.count(old)
    if hits!=1:raise RuntimeError(f'v0.9.19 integration source drift: {old[:100]!r}: {hits} matches')
    return s.replace(old,new,1)

s=p.read_text()
if s.count('#define FW_BUILD 0x0914u')!=1:raise RuntimeError('Expected exact experimental RC 0x0914 identity')
s=one(s,'#include "tim1_emergency_stop.h"',
      '#include "tim1_emergency_stop.h"\n#include "g30_adc_proof.h"')
s=one(s,'#define FW_BUILD 0x0914u',
      '#define FW_BUILD 0x0919u')
# Add only READ-ONLY pages. The E7-only compile guard from Motor RC
# stays unchanged. Old Android build guard must not match 0x0919.
anchor='    case 0xDAu: case 0xDBu: case 0xDCu: case 0xDDu: case 0xDEu: case 0xDFu:'
addition='''    case 0xFAu: case 0xFBu: { /* read-only ADC1/2 raw-and-offset evidence */
        /* Immutable snapshot from one IRQ-critical section; never read
         * half-old / half-new ADC offsets in one Ninebot BLE reply. */
        g30_adc_proof_input_t x={0};
        uint16_t bdtr,vbus_raw,ilast,ipeak,fault;
        uint32_t primask_before;
        __asm volatile("mrs %0, primask" : "=r"(primask_before) :: "memory");
        irq_disable();
        for(unsigned i=0u;i<3u;i++){
            x.raw[i]=g_fixed_scan.raw[i];
            x.off[i]=g_adc_offset[i];
        }
        x.adc2_ch4_off=g_adc2_ch4_offset;
        x.adc2_ch5_off=g_adc2_ch5_offset;
        x.ccer=(uint16_t)TIM_CCER(TIM1_BASE);
        bdtr=(uint16_t)TIM_BDTR(TIM1_BASE);
        x.have_sample=(uint8_t)(g_adc_samples>0u);
        x.adc1_offsets_valid=(uint8_t)(g_offset_cal_valid &&
                                            g_fixed_scan.offset_valid);
        x.adc2_ch4_valid=g_adc2_ch4_offset_valid;
        x.adc2_ch5_valid=g_adc2_ch5_offset_valid;
        x.killed=g_gate_kill_latched;x.powered=g_power_armed;
        x.now_ms=g_ms;x.last_adc_ms=g_adc_last_ms;
        vbus_raw=g_adc_raw[3];
        ilast=(uint16_t)g_last_abs_current;
        ipeak=(uint16_t)g_peak_abs_current;
        fault=(uint16_t)g_safety_latched;
        if(!(primask_before&1u))irq_enable();
        const g30_adc_proof_result_t o=g30_adc_proof_eval(&x);
        if(cmd==0xFAu)g30_adc_proof_page_fa(p,&x,o,fault);
        else g30_adc_proof_page_fb(p,&x,o,bdtr,vbus_raw,ilast,ipeak);
        n=18u;break;
    }
'''
s=one(s,anchor,addition+anchor)
# Keep all original motor/FOC wiring; only observational BLE code added.
assert s.count('case 0xFAu: case 0xFBu:')==1
assert '#error "Motor RC permits E7-only Motor-ID, never drive or sensorless E5"' in s
assert 'motor_id_bench_coast();' in s
assert 'TIM_BDTR(TIM1_BASE)&=(uint32_t)~D_GUARD_MOE;' in s
assert '#define COMM_CURRENT_SCALE_HW_VALID 0u' in s
assert '#define COMM_ADC_TIMING_HW_VALID 0u' in s
assert 'const motor_id_auto_io_t id_io={motor_id_hw_vector_mv,motor_id_hw_gate_off,0};' in s
assert s.count('g_gate_kill_latched=1u;')==1
p.write_text(s)
old=m.read_text()
if 'v0_9_14_rc' not in old:raise RuntimeError('Wrong Motor RC base version in Makefile')
m.write_text(old.replace('v0_9_14_rc','v0_9_19'))
dest=target/'src/g30_adc_proof.h'
if dest.exists():raise RuntimeError('Prior ADC proof already present')
shutil.copyfile(root/'v0.9.18/src/g30_adc_proof.h',dest)
print('PASS: Motor RC E7/E6 unchanged, FA/FB read-only and distinct FW 0x0919')
