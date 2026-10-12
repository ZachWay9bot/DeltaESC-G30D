#!/usr/bin/env python3
"""DeltaESC v0.9.20: compile the full Hall-free sensorless path while keeping
physical bridge arming impossible.

Apply only after v0.9.19 reconstruction. This is a source/link integration
step, not a motor-release step. It deliberately enables SENSORLESS_RUN_ALLOWED
for compilation while POWER_STAGE_ARM_ALLOWED remains zero.
"""
from pathlib import Path
import sys

target=Path(sys.argv[1]).resolve()
main=target/'src/main.c'
make=target/'Makefile'

def one(s,old,new,label):
    hits=s.count(old)
    if hits != 1:
        raise RuntimeError(f'v0.9.20 {label} drift: expected 1 hit, got {hits}')
    return s.replace(old,new,1)

s=main.read_text()
s=one(s,'#define FW_BUILD 0x0919u','#define FW_BUILD 0x0920u','build id')
s=one(s,
'''#if POWER_STAGE_ARM_ALLOWED || SENSORLESS_RUN_ALLOWED
#error "Motor RC permits E7-only Motor-ID, never drive or sensorless E5"
#endif''',
'''/* v0.9.20 compiles the complete sensorless software path for Cortex-M3,
 * but the physical bridge remains compile-locked. E5 therefore cannot arm
 * the G30 power stage in this target. */
#if POWER_STAGE_ARM_ALLOWED
#error "v0.9.20 SENSORLESS-LOCKED: physical gate arm forbidden"
#endif''',
'sensorless compile lock')

s=one(s,
'''#if SENSORLESS_RUN_ALLOWED || POWER_STAGE_ARM_ALLOWED
#if !COMM_CURRENT_SCALE_HW_VALID || !COMM_ADC_TIMING_HW_VALID
#error "DRV126 gate profile ready; powered build still requires valid ADC timing and current scaling"
#endif
#endif''',
'''/* SENSORLESS_RUN_ALLOWED may be compiled for source/link proof with the
 * bridge hard-disabled. Only a build that can physically arm the bridge must
 * satisfy the not-yet-qualified current-scale and ADC-timing gates. */
#if POWER_STAGE_ARM_ALLOWED
#if !COMM_CURRENT_SCALE_HW_VALID || !COMM_ADC_TIMING_HW_VALID
#error "DRV126 gate profile ready; powered build still requires valid ADC timing and current scaling"
#endif
#endif''',
'powered hardware qualification guard')

# Do not weaken any of the terminal shutdown or unqualified-hardware guards.
required=[
    'g_gate_kill_latched=1u;',
    'TIM_BDTR(TIM1_BASE)&=(uint32_t)~D_GUARD_MOE;',
    '#define COMM_CURRENT_SCALE_HW_VALID 0u',
    '#define COMM_ADC_TIMING_HW_VALID 0u',
    'case 0xFAu: case 0xFBu:',
    'sensorless_control_stop(&ctrl);',
]
for x in required:
    if x not in s:
        raise RuntimeError('v0.9.20 missing inherited safety contract: '+x)

main.write_text(s)

m=make.read_text()
if 'v0_9_19' not in m:
    raise RuntimeError('v0.9.20 expected v0_9_19 Makefile identity')
m=m.replace('v0_9_19','v0_9_20')
if 'sensorless-locked:' in m:
    raise RuntimeError('v0.9.20 target already present')

m += r'''

# v0.9.20 source/link proof only. There is intentionally no .bin target:
# this target must not become an easy-to-flash motor image.
SENSORLESS_LOCKED_ELF = build/DeltaESC_G30D_v0_9_20_sensorless_locked.elf
sensorless-locked: $(SENSORLESS_LOCKED_ELF)
$(SENSORLESS_LOCKED_ELF): $(SRC) linker.ld | build
	$(CC) $(CFLAGS_COMMON) -DPOWER_STAGE_ARM_ALLOWED=0 -DSENSORLESS_RUN_ALLOWED=1 -DSENSORLESS_CLOSED_LOOP_ALLOWED=1 -DMOTOR_ID_BENCH_ALLOWED=0 $(SRC) $(LDFLAGS_COMMON) -Wl,--undefined=sensorless_control_step -Wl,--undefined=vesc_ebics_flux_step -Wl,-Map=build/v0_9_20_sensorless_locked.map -o $@
	$(OBJDUMP) -h $@ > build/v0_9_20_sensorless_locked_sections.txt
'''
make.write_text(m)
print('PASS: v0.9.20 full sensorless Cortex-M3 path linked with physical bridge compile-locked')
