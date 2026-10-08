#!/usr/bin/env python3
"""Compiled ARM USART2/Ninebot ABI for v0.8.2 current frontend, synthetic MMIO only."""
import importlib.util,struct,sys
from pathlib import Path
path=Path(__file__).resolve().parents[2]/'v0.6.10'/'full_target_protocol.py'
spec=importlib.util.spec_from_file_location('deltaesc_arm_path',path)
mod=importlib.util.module_from_spec(spec);spec.loader.exec_module(mod)
t=mod.Target(Path(sys.argv[1]).resolve())
out=t.packet(1,b'\x10',arg=0xD0)
assert out[2]==16 and out[7:11]==b'DESC' and out[13:15]==b'\x02\x08',out.hex()
for reg in range(0xDA,0xE0):
    out=t.packet(1,b'\x10',arg=reg)
    assert out[:2]==b'\x5a\xa5' and out[2]==16 and out[5]==4 and out[6]==reg,out.hex()
    outw=t.packet(2,b'\0\0',arg=reg)
    assert outw[5]==5 and outw[7]==4,('read-only motor page write accepted',reg,outw.hex())
out=t.packet(1,b'\x10',arg=0xD9)
assert out[2]==16 and out[5]==4 and out[6]==0xD9,out.hex()
p=out[7:-2]
guard=struct.unpack_from('<H',p,6)[0]
assert (guard & 0x7000)==0x7000,hex(guard)
out=t.packet(2,b'\xDE\xC0',arg=0xE1)
assert out[5]==5 and out[6]==0xE1 and out[7]==2,out.hex()
for reg,val in ((0xF0,100000),(0xF1,100000),(0xF2,12000)):
    out=t.packet(2,struct.pack('<I',val),arg=reg)
    assert out[5]==5 and out[6]==reg and out[7]==0,out.hex()
    out=t.packet(1,b'\x04',arg=0xD3+reg-0xF0)
    assert struct.unpack('<I',out[7:11])[0]==val,out.hex()
out=t.packet(1,b'\x10',arg=0xD9);p=out[7:-2];guard=struct.unpack_from('<H',p,6)[0]
assert (guard & 0x7000)==0x7000,hex(guard)
print('PASS actual ARM UART: D0 0x0802, D9 guard mask, E1 preflight UNSAFE, F0-F2 RAM model, DA-DF read-only')
