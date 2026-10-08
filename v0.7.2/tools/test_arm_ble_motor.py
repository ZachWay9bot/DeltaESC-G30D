#!/usr/bin/env python3
"""Real compiled ARM USART2/Ninebot READ/ACK path, only synthetic MMIO."""
import importlib.util, os, sys
from pathlib import Path
path=Path(__file__).resolve().parents[2]/'v0.6.10'/'full_target_protocol.py'
spec=importlib.util.spec_from_file_location('deltaesc_arm_path',path)
mod=importlib.util.module_from_spec(spec)
spec.loader.exec_module(mod)
t=mod.Target(Path(sys.argv[1]).resolve())
for reg in range(0xDA,0xE0):
    out=t.packet(1,b'\x10',arg=reg)
    assert out[:2]==b'\x5a\xa5' and out[2]==16 and out[5]==4 and out[6]==reg,out.hex()
    payload=out[7:-2]
    if reg==0xDA:
        assert payload[:4]==b'MTR0' and payload[8:12]==b'\0'*4,payload.hex()
    if reg==0xDD:
        assert payload[6:8]==b'\x00\x01' and payload[8:12]==b'\0'*4,payload.hex()
    if reg==0xDF:
        assert payload[12:16]==b'\0\0\x01\0',payload.hex()
out=t.packet(2,b'\0\0',arg=0xDA)
assert out[5]==5 and out[7]==4,('read-only register write unexpectedly accepted',out.hex())
out=t.packet(1,b'\x10',arg=0xD0)
assert out[2]==16 and out[7:11]==b'DESC' and out[15:17]==b'\x02\x07',out.hex()
print('PASS actual ARM/Ninebot UART: DA-DF 16B reads; writes rejected; D0 unchanged')
