#!/usr/bin/env python3
import struct
from pathlib import Path
APP_BASE=0x08001000; APP_LIMIT=0x0800E000
src=(Path('src/ninebot_link.c').read_text()+Path('src/main.c').read_text())
assert '0x5Au' in src and '0xA5u' in src, 'G30 5A A5 parser missing'
assert 'NINEBOT_FMT_55AA' in src, '55AA compatibility fallback missing'
assert "p[0]='D';p[1]='E';p[2]='S';p[3]='C'" in src, 'D0 DeltaESC signature missing'
assert '#define FW_BUILD 0x0603u' in src
assert '0x08001002' not in Path('src/iap_update.c').read_text(), 'old vector invalidation source present'
for p in sorted(Path('build').glob('*.bin')):
    b=p.read_bytes(); assert 8<=len(b)<=APP_LIMIT-APP_BASE
    sp,reset=struct.unpack_from('<II',b,0)
    assert 0x20000000<=sp<=0x20005000
    assert reset&1 and APP_BASE<=(reset&~1)<APP_LIMIT
    assert struct.pack('<I',0x0800E800) in b, f'{p}: missing staging base'
    assert struct.pack('<I',0x0801F800) in b, f'{p}: missing control base'
    assert struct.pack('<I',0x08001002) not in b, f'{p}: vector invalidation literal present'
    assert b'v0.6.3 G30-framing + staged-IAP firmware' in b
    print(p.name,'PASS',len(b),hex(sp),hex(reset))
print('v0.6.3 G30 framing + staged-IAP preflight: PASS')
