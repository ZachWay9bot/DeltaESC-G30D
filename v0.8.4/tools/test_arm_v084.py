#!/usr/bin/env python3
"""Actual compiled Cortex-M3 USART2 Ninebot round trip for v0.8.4."""
import importlib.util,struct,sys
from pathlib import Path
repo=Path(__file__).resolve().parents[2]
spec=importlib.util.spec_from_file_location('deltaesc_arm',repo/'v0.6.10'/'full_target_protocol.py')
mod=importlib.util.module_from_spec(spec);spec.loader.exec_module(mod)
t=mod.Target(Path(sys.argv[1]).resolve())
magic=struct.pack('<H',0xC0DE)
def read(reg,n):
    out=t.packet(1,bytes([n]),arg=reg)
    assert out[5:7]==bytes([4,reg]) and out[2]==n,out.hex()
    return out[7:7+n]
def write(reg,p):
    out=t.packet(2,p,arg=reg)
    assert out[5:7]==bytes([5,reg]) and out[2]==1,out.hex()
    return out[7]
def val(reg):
    return struct.unpack('<I',read(reg,4))[0]
assert read(0xD0,16)[6:8]==b'\x04\x08'
assert read(0xE7,16)[0:4]==b'\0\0\0\0'
assert val(0xD3)==val(0xD4)==val(0xD5)==0
assert write(0xF0,struct.pack('<I',250000))==3
assert write(0xF5,magic)==5 # Incomplete transaction
for reg,value in ((0xF0,250000),(0xF1,180000),(0xF2,12000)):
    assert write(reg,magic+struct.pack('<I',value))==6
assert read(0xE7,16)[0]==7 and read(0xE7,16)[1]==0
assert val(0xD3)==val(0xD4)==val(0xD5)==0 # No partial observer modification
assert write(0xF3,magic+struct.pack('<h',-1234))==6
assert write(0xF4,magic+struct.pack('<H',350))==6
st=read(0xE7,16)
assert st[0]==0x1f and st[1]==0,st.hex()
assert write(0xF5,magic+b'\x01')==5
assert write(0xF5,magic)==6
st=read(0xE7,16)
assert st[0]==0 and st[1]==1 and st[2]==1 and st[3]==0,st.hex()
assert val(0xD3)==250000 and val(0xD4)==180000 and val(0xD5)==12000
assert struct.unpack('<I',st[8:12])[0]==250000
# Verify firmware still refuses the first drive regardless of valid params.
assert write(0xE1,magic)==2
assert write(0xE5,magic+struct.pack('<H',100))==4
# E7 write is never a valid configuration command.
assert write(0xE7,magic)==4
# Aborting after partially staging does not change active values.
assert write(0xF0,magic+struct.pack('<I',300000))==6
assert read(0xE7,16)[0]==1 and val(0xD3)==250000
assert write(0xF5,magic+b'\0')==0
assert read(0xE7,16)[0]==0 and val(0xD3)==250000
print('PASS ARM v0.8.4: exact build, guarded stage/commit, E7 status, reject partial/no magic, E1/E5 blocked')
