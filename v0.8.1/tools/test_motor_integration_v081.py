#!/usr/bin/env python3
import os,subprocess,sys
from pathlib import Path
root=Path(sys.argv[1]).resolve();main=(root/'src/main.c').read_text();core=(root/'src/sensorless_control.c').read_text();mk=(root/'Makefile').read_text()
assert '#define FW_BUILD 0x0801u' in main
assert '#error "v0.8.1 BLE transactional config must NEVER arm a physical power stage"' in main
assert 'all: safe\n' in mk and 'active: ' not in mk and 'bench: ' not in mk
assert '-DPOWER_STAGE_ARM_ALLOWED=0 -DSENSORLESS_RUN_ALLOWED=0' in mk
assert 'motor_config_pending_complete(&g_motor_cfg)' in main
assert 'get_u32(f->payload+2)' in main and 'if(!has_magic(f))' in main
assert 'ctrl.bemf_mv' in main and 'ctrl.flux_sq' not in main
assert 's->motor_params_valid && s->observer_valid' in core and 'expected_step' in core and 'speed_flux_ok' in core
assert '(va_mv - ra_mv - la_mv) << 8' not in core and '(vb_mv - rb_mv - lb_mv) << 8' not in core
binary=root/'build/DeltaESC_G30D_v0_8_1_ble_syncsafe.bin';assert binary.is_file() and 0<binary.stat().st_size<=(0xE000-0x1000)
cc=os.environ.get('HOST_CC','clang');out=root/'build'
for name,defs in (('voltage',['-DMOTOR_VOLTAGE_BENCH=1','-DSENSORLESS_CLOSED_LOOP_ALLOWED=0']),('foc',['-DMOTOR_VOLTAGE_BENCH=0','-DSENSORLESS_CLOSED_LOOP_ALLOWED=0']),('sensorless',['-DMOTOR_VOLTAGE_BENCH=0','-DSENSORLESS_CLOSED_LOOP_ALLOWED=1'])):
    exe=out/('host_motor_'+name)
    subprocess.check_call([cc,'-std=c11','-O1','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-fsanitize=undefined','-fno-sanitize-recover=all','-Isrc',*defs,'tools/motor_core_host_test.c','src/sensorless_control.c','-o',str(exe)],cwd=root)
    subprocess.check_call([str(exe)])
subprocess.check_call([cc,'-std=c11','-O1','-Wall','-Wextra','-Werror','-fsanitize=undefined','-fno-sanitize-recover=all','-Isrc','src/motor_probe.c','src/ble_motor_probe.c','tools/test_ble_motor_probe.c','-o',str(out/'host_motor_ble')],cwd=root)
subprocess.check_call([str(out/'host_motor_ble')])
subprocess.check_call([sys.executable,'tools/test_motor_config_txn.py'],cwd=root,env={**os.environ,'HOST_CC':cc})
print('PASS v0.8.1: sensorless core + BLE ADC + transactional motor config, gates compile-disabled')
