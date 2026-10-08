#!/usr/bin/env python3
"""Exercise compiled ARM v0.8.1 F0-F5 transaction through real Ninebot USART2 parser."""
import importlib.util, struct, sys
from pathlib import Path
repo=Path(__file__).resolve().parents[2]
path=repo/'v0.6.10'/'full_target_protocol.py'
spec=importlib.util.spec_from_file_location('deltaesc_arm_path',path)
mod=importlib.util.module_from_spec(spec);spec.loader.exec_module(mod)
t=mod.Target(Path(sys.argv[1]).resolve())
magic=struct.pack('<H',0xC0DE)

def pkt_write(reg,payload):
    out=t.packet(2,payload,arg=reg)
    assert out[5]==5 and out[6]==reg,out.hex()
    return out[7]
def read_u32(reg):
    out=t.packet(1,b'\x04',arg=reg)
    assert out[5]==4 and out[6]==reg,out.hex()
    return struct.unpack('<I',out[7:11])[0]
def read_d9():
    out=t.packet(1,b'\x0c',arg=0xD9)
    assert out[2]==12 and out[5:7]==b'\x04\xd9',out.hex()
    p=out[7:-2]
    return struct.unpack('<HHHhHBB',p)

# Build identity and safe default.
out=t.packet(1,b'\x10',arg=0xD0)
assert out[7:11]==b'DESC' and out[13:15]==b'\x01\x08',out.hex()
assert read_u32(0xD3)==0 and read_u32(0xD4)==0 and read_u32(0xD5)==0
assert pkt_write(0xF0,struct.pack('<I',250000))==3,'write without magic was accepted'

vals={0xF0:250000,0xF1:180000,0xF2:12000}
for reg,val in vals.items():
    assert pkt_write(reg,magic+struct.pack('<I',val))==6
# Staging must not alter active readback.
assert read_u32(0xD3)==0 and read_u32(0xD4)==0 and read_u32(0xD5)==0
assert pkt_write(0xF3,magic+struct.pack('<h',-1234))==6
assert pkt_write(0xF4,magic+struct.pack('<H',350))==6
fault,diag,oc,phase,current,mask,valid=read_d9()
assert phase==0 and current==500 and mask==0x1f and valid==0,(phase,current,mask,valid)
# Commit only with complete tuple; trailing bytes are rejected.
assert pkt_write(0xF5,magic+b'\x01')==5
assert read_u32(0xD3)==0
assert pkt_write(0xF5,magic)==6
assert read_u32(0xD3)==250000 and read_u32(0xD4)==180000 and read_u32(0xD5)==12000
fault,diag,oc,phase,current,mask,valid=read_d9()
assert phase==-1234 and current==350 and mask==0 and valid==1,(phase,current,mask,valid)
# A new partial stage cannot change active values and can be aborted explicitly.
assert pkt_write(0xF0,magic+struct.pack('<I',260000))==6
assert read_u32(0xD3)==250000
assert read_d9()[-2]==0x01
assert pkt_write(0xF5,magic+b'\x00')==0
assert read_d9()[-2]==0 and read_u32(0xD3)==250000
# Range checks remain fail-closed.
assert pkt_write(0xF4,magic+struct.pack('<H',99))==5
print('PASS actual ARM v0.8.1: magic-gated staging, no partial apply, atomic RAM commit/readback, abort, range refusal')
