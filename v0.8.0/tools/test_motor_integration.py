#!/usr/bin/env python3
"""Check integrated sensorless FOC source + proven BLE contract, WITHOUT enabling gates."""
import sys,subprocess,os
from pathlib import Path
root=Path(sys.argv[1]).resolve()
main=(root/'src/main.c').read_text()
core=(root/'src/sensorless_control.c').read_text()
make=(root/'Makefile').read_text()
assert '#define FW_BUILD 0x0800u' in main
assert '#error "v0.8.0 integrated motor candidate must NEVER arm a physical power stage"' in main
assert 'all: safe\n' in make and 'active: ' not in make and 'bench: ' not in make
assert '-DPOWER_STAGE_ARM_ALLOWED=0 -DSENSORLESS_RUN_ALLOWED=0' in make
assert 'case 0xDAu: case 0xDBu: case 0xDCu: case 0xDDu: case 0xDEu: case 0xDFu:' in main
assert 'apply_motor_params_if_complete()' in main
assert 'sensorless_control_set_motor_params(&ctrl' in main
for name in ('g_cfg_r_uohm','g_cfg_l_nh','g_cfg_flux_uwb'):
    assert name+'=v;g_cfg_dirty=1u;apply_motor_params_if_complete();' in main
assert 'ctrl.bemf_mv' in main and 'ctrl.flux_sq' not in main
assert 's->motor_params_valid && s->observer_valid' in core
assert 'expected_step' in core and 'speed_flux_ok' in core
assert 'int32_t id_ref = (s->state == SENSORLESS_ALIGN) ? s->run_iq_counts : 0;' in core
assert '(va_mv - ra_mv - la_mv) << 8' not in core
assert '(vb_mv - rb_mv - lb_mv) << 8' not in core
binary=root/'build/DeltaESC_G30D_v0_8_0_ble_syncsafe.bin'
assert binary.is_file() and 0<binary.stat().st_size<=(0xE000-0x1000)
out=root/'build'
cc=os.environ.get('HOST_CC','clang')
for name,defs in (
    ('voltage',['-DMOTOR_VOLTAGE_BENCH=1','-DSENSORLESS_CLOSED_LOOP_ALLOWED=0']),
    ('foc',['-DMOTOR_VOLTAGE_BENCH=0','-DSENSORLESS_CLOSED_LOOP_ALLOWED=0']),
    ('sensorless',['-DMOTOR_VOLTAGE_BENCH=0','-DSENSORLESS_CLOSED_LOOP_ALLOWED=1'])
):
    exe=out/('host_motor_'+name)
    subprocess.check_call([cc,'-std=c11','-O1','-Wall','-Wextra','-Werror',
                           '-Wno-misleading-indentation','-fsanitize=undefined',
                           '-fno-sanitize-recover=all','-Isrc',*defs,
                           'tools/motor_core_host_test.c','src/sensorless_control.c',
                           '-o',str(exe)],cwd=root)
    subprocess.check_call([str(exe)])
subprocess.check_call([cc,'-std=c11','-O1','-Wall','-Wextra','-Werror',
                       '-fsanitize=undefined','-fno-sanitize-recover=all',
                       '-Isrc','src/motor_probe.c','src/ble_motor_probe.c',
                       'tools/test_ble_motor_probe.c','-o',str(out/'host_motor_ble')],cwd=root)
subprocess.check_call([str(out/'host_motor_ble')])
print('PASS: integrated R/L/flux observer, D0 0x0800, DA-DF phone telemetry, all gates off, 3x motor UBSan + BLE host tests')
