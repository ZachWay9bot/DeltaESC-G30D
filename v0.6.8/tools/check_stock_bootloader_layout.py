#!/usr/bin/env python3
"""Read-only local full-dump layout check. Never uploads the dump."""
import argparse, hashlib, struct
from pathlib import Path

ap=argparse.ArgumentParser()
ap.add_argument('dump',type=Path,help='locally held 128-KiB G30D ESC full dump')
a=ap.parse_args()
b=a.dump.read_bytes()
assert len(b)==131072,f'Expected 131072 bytes, got {len(b)}'
print('local dump sha256:',hashlib.sha256(b).hexdigest())
sp,reset=struct.unpack_from('<II',b,0)
assert 0x20000000<=sp<=0x20005000 and reset&1 and 0x08000000<reset<0x08001000
print(f'bootloader vector: SP={sp:08X} RESET={reset:08X}')
for offset in (0x1F800,0x1FC00):
    magic,flag,size=struct.unpack_from('<III',b,offset)
    print(f'flash 0x{0x08000000+offset:08X}: magic=0x{magic:08X}, flag={flag}, image_size={size}')
    assert magic==0x0000505A
boot=b[:4096]
for addr in (0x0801F800,0x0801FC00,0x0800E800,0x08001000):
    needle=struct.pack('<I',addr)
    offs=[i for i in range(len(boot)-3) if boot[i:i+4]==needle]
    print(f'bootloader exact literal 0x{addr:08X}: {len(offs)} offsets: {[hex(o) for o in offs]}')
assert boot.count(struct.pack('<I',0x0801F800))>=2
assert boot.count(struct.pack('<I',0x0800E800))>=1
print('PASS: layout observations only. No direct literal does NOT rule out indirect references.')
