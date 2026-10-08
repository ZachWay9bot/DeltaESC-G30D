#!/usr/bin/env python3
"""Host-level ABI and guard tests for smartphone BLE motor telemetry."""
import sys, subprocess,os
from pathlib import Path
root=Path(sys.argv[1]).resolve()
main=(root/'src/main.c').read_text()
make=(root/'Makefile').read_text()
assert 'case 0xDAu: case 0xDBu: case 0xDCu: case 0xDDu: case 0xDEu: case 0xDFu:' in main
assert 'n=ble_motor_probe_read(cmd,p)' in main
assert 'FW_BUILD 0x0702u' in main
assert '#error "v0.7.2 BLE motor telemetry must NEVER arm a physical power stage"' in main
assert '-DPOWER_STAGE_ARM_ALLOWED=0 -DSENSORLESS_RUN_ALLOWED=0' in make
assert 'all: safe\n' in make and 'active: ' not in make and 'bench: ' not in make
assert 'src/ble_motor_probe.c' in make
assert 'else if(f->arg>=0xF0u&&f->arg<=0xF5u)' in main
assert 'NINEBOT_READ_ACK' in main
binary=root/'build/DeltaESC_G30D_v0_7_2_ble_syncsafe.bin'
assert binary.is_file() and binary.stat().st_size<=0xc800
cc=os.getenv('HOST_CC','cc')
cmd=[cc,'-std=c11','-O2','-g','-Wall','-Wextra','-Werror','-fsanitize=undefined',
    '-fno-sanitize-recover=all','-I'+str(root/'src'),
    str(root/'src/motor_probe.c'),str(root/'src/ble_motor_probe.c'),
    str(Path(__file__).with_name('test_ble_motor_probe.c')),'-o','/tmp/test_ble_motor_probe']
subprocess.check_call(cmd)
subprocess.check_call(['/tmp/test_ble_motor_probe'])
print('PASS v0.7.2 BLE motor telemetry host tests / compiled gate-OFF ABI')
