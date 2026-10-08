#!/usr/bin/env python3
"""Keep the v0.7.1 PA2 BLE transport, publish motor probe read-only via D[A-F]."""
from pathlib import Path
import sys
root=Path(sys.argv[1])
def patch_once(s,old,new):
    n=s.count(old)
    if n!=1:raise RuntimeError(f'expected exactly one marker (found {n}): {old[:100]!r}')
    return s.replace(old,new,1)

main=root/'src/main.c'
s=main.read_text()
s=patch_once(s,'#include "motor_probe.h"','#include "motor_probe.h"\n#include "ble_motor_probe.h"')
s=patch_once(s,'#define FW_BUILD 0x0701u','#define FW_BUILD 0x0702u')
s=patch_once(s,'v0.7.1 FOC PI math audit must NEVER arm a physical power stage',
             'v0.7.2 BLE motor telemetry must NEVER arm a physical power stage')
s=patch_once(s,'v0.7.1 PASSIVE MOTOR PI MATH AUDIT','v0.7.2 DASHBOARD BLE MOTOR TELEMETRY')
old='case 0xD9u: put_u16(p,current_fault_code());put_u16(p+2,diag_flags());put_u16(p+4,HARD_OC_COUNTS);n=6u;break;'
new=old+'\n    case 0xDAu: case 0xDBu: case 0xDCu: case 0xDDu: case 0xDEu: case 0xDFu:\n        n=ble_motor_probe_read(cmd,p);break;'
s=patch_once(s,old,new)
# No new write paths: the 0xDA..DF read-only pages inherit the same
# Ninebot READ -> READ_ACK 0x04, PA2 USART2, BLE Crypto and the original app.
assert 'else if(f->arg>=0xF0u&&f->arg<=0xF5u)' in s
assert 'else if(f->arg>=0xE0u&&f->arg<=0xE6u)' in s
assert 'case 0xD0u:' in s and 'case 0xD9u:' in s
main.write_text(s)

mk=root/'Makefile'
s=mk.read_text()
s=patch_once(s,'src/motor_probe.c','src/motor_probe.c src/ble_motor_probe.c')
assert 'all: safe\n' in s and 'active: ' not in s and 'bench: ' not in s
assert '-DPOWER_STAGE_ARM_ALLOWED=0 -DSENSORLESS_RUN_ALLOWED=0' in s
assert 'v0_7_1' in s
mk.write_text(s.replace('v0_7_1','v0_7_2'))
for name in ['ble_motor_probe.c','ble_motor_probe.h']:
    (root/'src'/name).write_text((Path(__file__).resolve().parent/'src'/name).read_text())
print('v0.7.2: DA-DF motor pages on existing Ninebot BLE READ/ACK; no new link or motor output')
