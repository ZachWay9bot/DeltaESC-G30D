#!/usr/bin/env python3
"""No serial ports: verify packet ABI against unchanged C parser layout."""
from G30_Config_Packets import build_packets, ninebot

frames=build_packets(280000,650000,16000,1234,500,15,800)
assert len(frames)==7
assert [p[6] for _,p in frames]==[0xf0,0xf1,0xf2,0xf3,0xf4,0xf5,0xff]
assert [p[2] for _,p in frames]==[6,6,6,4,4,2,5]
for label,p in frames:
    assert p[:2]==b"\x5a\xa5"
    assert p[3:6]==b"\x3d\x20\x03"
    assert p[7:9]==b"\xde\xc0"
    assert len(p)==p[2]+9
    check=(~sum(p[2:-2]))&0xffff
    assert p[-2:] == check.to_bytes(2,"little"),label
assert frames[-1][1][9:12]==bytes([15,0x20,0x03])
for args in [
    (0,650000,16000,1234,500,15,800),
    (280000,0,16000,1234,500,15,800),
    (280000,650000,0,1234,500,15,800),
    (280000,650000,16000,32768,500,15,800),
    (280000,650000,16000,1234,2100,15,800),
    (280000,650000,16000,1234,500,0,800),
    (280000,650000,16000,1234,500,15,0),
]:
    try: build_packets(*args)
    except ValueError: pass
    else: raise AssertionError("Invalid motor profile accepted")
print("PASS packet ABI: F0-F5/FF order, magic, length, checksum, measured-only range")
