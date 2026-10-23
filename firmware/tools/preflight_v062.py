#!/usr/bin/env python3
import struct,sys
from pathlib import Path
APP_BASE=0x08001000; APP_LIMIT=0x0800E000
for p in sorted(Path('build').glob('*.bin')):
    b=p.read_bytes(); assert 8<=len(b)<=APP_LIMIT-APP_BASE
    sp,reset=struct.unpack_from('<II',b,0); assert 0x20000000<=sp<=0x20005000; assert reset&1; assert APP_BASE<=(reset&~1)<APP_LIMIT
    assert struct.pack('<I',0x0800E800) in b, f'{p}: missing staging base'
    assert struct.pack('<I',0x0801F800) in b, f'{p}: missing control base'
    assert struct.pack('<I',0x08001002) not in b, f'{p}: old vector invalidation literal present'
    assert b'v0.6.2 staged-IAP firmware' in b
    print(p.name,'PASS',len(b),hex(sp),hex(reset))
print('v0.6.2 staged-IAP preflight: PASS')
