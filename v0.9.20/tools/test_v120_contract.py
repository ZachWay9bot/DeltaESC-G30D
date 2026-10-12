#!/usr/bin/env python3
"""Fail-closed source contract for v0.9.20 sensorless-locked integration."""
from pathlib import Path
import sys

root=Path(sys.argv[1]).resolve()
main=(root/'src/main.c').read_text()
make=(root/'Makefile').read_text()
obs=(root/'src/vesc_ebics_flux.c').read_text()
ctl=(root/'src/sensorless_control.c').read_text()
auto=(root/'src/motor_id_auto.c').read_text()
tests=(root/'tools/run_v114_tests.sh').read_text()

def req(x,msg):
    if not x:
        raise SystemExit('FAIL: '+msg)

req(main.count('#define FW_BUILD 0x0920u')==1,'unique 0x0920 identity')
req('#if POWER_STAGE_ARM_ALLOWED' in main,'physical arm compile lock present')
req('v0.9.20 SENSORLESS-LOCKED: physical gate arm forbidden' in main,'arm rejection message')
req('SENSORLESS_LOCKED_ELF' in make and 'sensorless-locked:' in make,'locked ELF target')
req('-DPOWER_STAGE_ARM_ALLOWED=0' in make,'locked target cannot arm bridge')
req('-DSENSORLESS_RUN_ALLOWED=1' in make,'sensorless run code compiled')
req('-DSENSORLESS_CLOSED_LOOP_ALLOWED=1' in make,'observer closed-loop code compiled')
req('sensorless_control_step' in make and 'vesc_ebics_flux_step' in make,'sensorless symbols forced into ELF')
locked=make.split('# v0.9.20 source/link proof only.',1)[1]
req('SENSORLESS_LOCKED_BIN' not in locked and
    'DeltaESC_G30D_v0_9_20_sensorless_locked.bin' not in locked,
    'locked target must not define a flashable BIN')
req('g_gate_kill_latched=1u;' in main,'terminal kill latch inherited')
req('TIM_BDTR(TIM1_BASE)&=(uint32_t)~D_GUARD_MOE;' in main,'terminal MOE clear inherited')
req('#define COMM_CURRENT_SCALE_HW_VALID 0u' in main,'physical current gain remains unqualified')
req('#define COMM_ADC_TIMING_HW_VALID 0u' in main,'physical ADC timing remains unqualified')
req('case 0xFAu: case 0xFBu:' in main,'read-only ADC proof retained')
req('vesc_ebics_flux_step' in obs,'VESC/EBiCS observer retained')
req('SENSORLESS_HANDOVER' in ctl and 'SENSORLESS_CLOSED_LOOP' in ctl,
    'STOP/OPEN/HANDOVER/CLOSED sensorless state machine retained')
req('sensorless_control_t staged_control' in auto and
    'motor_id_commit(&a->id,&staged_control)' in auto and
    'copy_struct(a->control,&staged_control' in auto,
    'Motor-ID R/L/Flux adoption into sensorless control retained')
req('sensorless_plant_host_test.c' in tests,'synthetic sensorless plant regression retained')
req('motor_pipeline_host_test.c' in tests,'ADC-to-FOC pipeline regression retained')
req('vesc_ebics_flux_host_test.c' in tests,'observer regression retained')
req('hall' not in ctl.lower() and 'hall' not in obs.lower(),
    'no Hall dependency in sensorless observer/control core')
print('PASS: v0.9.20 is Hall-free sensorless software integration, not a gate-arm release')
