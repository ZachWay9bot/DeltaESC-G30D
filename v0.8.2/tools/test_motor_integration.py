#!/usr/bin/env python3
"""v0.8.1 static + host audit. Never enable physical gates."""
import sys,subprocess,os
from pathlib import Path
root=Path(sys.argv[1]).resolve()
main=(root/'src/main.c').read_text()
core=(root/'src/sensorless_control.c').read_text()
guard=(root/'src/commissioning_guard.c').read_text()
make=(root/'Makefile').read_text()
assert '#define FW_BUILD 0x0802u' in main
assert '#error "v0.8.1 BLE commissioning audit must NEVER arm a physical power stage"' in main
assert 'all: safe\n' in make and 'active: ' not in make and 'bench: ' not in make
assert '-DPOWER_STAGE_ARM_ALLOWED=0 -DSENSORLESS_RUN_ALLOWED=0' in make
assert 'src/commissioning_guard.c' in make\nassert 'src/stock_current_frontend.c' in make\nassert '#define COMM_STOCK_CURRENT_MODEL_VALID 1u' in main\nassert '#define COMM_CURRENT_SCALE_HW_VALID 0u' in main
assert 'case 0xDAu: case 0xDBu: case 0xDCu: case 0xDDu: case 0xDEu: case 0xDFu:' in main
assert 'case 0xD9u:' in main and 'put_u16(p+6,commissioning_guard_mask())' in main
assert 'if(cmd==0xE1u)' in main and 'commissioning_guard_mask()!=0u' in main
assert '#define COMM_CURRENT_SCALE_HW_VALID 0u' in main
assert '#define COMM_GATE_HW_VALID 0u' in main
assert '#define COMM_ADC_TIMING_HW_VALID 0u' in main
assert 'COMM_GUARD_CURRENT_SCALE_HW' in guard and 'COMM_GUARD_GATE_HW' in guard and 'COMM_GUARD_ADC_TIMING_HW' in guard
assert 'apply_motor_params_if_complete()' in main
assert 'sensorless_control_set_motor_params(&ctrl' in main
assert 'ctrl.bemf_mv' in main and 'ctrl.flux_sq' not in main
assert 's->motor_params_valid && s->observer_valid' in core
binary=root/'build/DeltaESC_G30D_v0_8_2_ble_syncsafe.bin'
assert binary.is_file() and 0<binary.stat().st_size<=(0xE000-0x1000)
out=root/'build';cc=os.environ.get('HOST_CC','clang')
for name,defs in (
    ('voltage',['-DMOTOR_VOLTAGE_BENCH=1','-DSENSORLESS_CLOSED_LOOP_ALLOWED=0']),
    ('foc',['-DMOTOR_VOLTAGE_BENCH=0','-DSENSORLESS_CLOSED_LOOP_ALLOWED=0']),
    ('sensorless',['-DMOTOR_VOLTAGE_BENCH=0','-DSENSORLESS_CLOSED_LOOP_ALLOWED=1'])
):
    exe=out/('host_motor_'+name)
    subprocess.check_call([cc,'-std=c11','-O1','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-fsanitize=undefined','-fno-sanitize-recover=all','-Isrc',*defs,'tools/motor_core_host_test.c','src/sensorless_control.c','-o',str(exe)],cwd=root)
    subprocess.check_call([str(exe)])
subprocess.check_call([cc,'-std=c11','-O1','-Wall','-Wextra','-Werror','-fsanitize=undefined','-fno-sanitize-recover=all','-Isrc','src/motor_probe.c','src/ble_motor_probe.c','tools/test_ble_motor_probe.c','-o',str(out/'host_motor_ble')],cwd=root)
subprocess.check_call([str(out/'host_motor_ble')])
print('PASS: v0.8.1 motor observer unchanged, phone commissioning guard installed, hardware gates still off')
