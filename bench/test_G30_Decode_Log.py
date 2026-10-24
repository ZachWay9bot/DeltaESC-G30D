#!/usr/bin/env python3
"""Synthetic fixtures for one-shot UART RX reconstruction; no hardware."""
import tempfile
from pathlib import Path
from G30_Decode_Log import decode, make_report, parse_stream

def frame(src,dst,cmd,arg,payload):
    h=bytes([0x5a,0xa5,len(payload),src,dst,cmd,arg])+payload
    ck=(~sum(h[2:]))&65535
    return h+ck.to_bytes(2,"little")

def block(size):
    return bytearray(size)
def set16(b,p,x,signed=False):
    b[p:p+2]=x.to_bytes(2,"little",signed=signed)

fw=block(16)
fw[:4]=b"DESC";fw[4:6]=bytes([0,2]);set16(fw,6,0x1030)
fw[8]=5;set16(fw,10,0);set16(fw,12,16000);set16(fw,14,4000)
fd=block(28)
fd[:4]=b"RC12";fd[4:8]=bytes([5,0,1,2]);set16(fd,12,-20,True)
set16(fd,14,-437,True);set16(fd,18,8,True);set16(fd,22,100)
fe=block(28)
fe[:4]=b"PI12";set16(fe,4,-5,True);set16(fe,6,7,True);set16(fe,8,-2,True)
set16(fe,18,12,True);fe[26]=0;fe[27]=1
d9=block(16)
set16(d9,6,(1<<3)|(1<<12)|(1<<14));d9[8]=1
ff=block(12);ff[:4]=b"PF13";ff[4:6]=bytes([15,5]);set16(ff,6,763);ff[10]=1;ff[11]=1
entries=[(0xD0,fw),(0xD9,d9),(0xFD,fd),(0xFE,fe),(0xFF,ff)]
raw=b"".join(frame(0x20,0x3d,4,cmd,p) for cmd,p in entries)
broken=bytearray(frame(0x20,0x3d,4,0xFE,fe))
broken[-1] ^= 0x80
decoded,n_bad=parse_stream(b"noise"+bytes(broken)+raw)
assert n_bad == 1
assert len(decoded)==5
views={x["register"]:x for x in map(decode,decoded)}
assert views["D0"]["build"]=="1030"
assert views["D0"]["control_hz"]==4000
assert views["FD"]["stop_reason"]=="brake -> coast"
assert views["FD"]["phase_error"]==-20
assert views["FD"]["electrical_step"]==-437
assert views["FE"]["phase_currents_raw_counts"]==[-5,7,-2]
assert views["FE"]["emergency_gate_kill_latched"] is True
assert views["D9"]["start_blockers"]==[
    "motor parameters invalid",
    "physical current scaling uncalibrated",
    "ADC sampling timing uncalibrated",
]
assert views["FF"]["pole_pairs"]==15
assert views["FF"]["drive_build"] is True
payload=frame(0x21,0x20,0x65,0,b"\x00\x00\x00")
assert decode(parse_stream(payload)[0][0])["note"].startswith("Not a DeltaESC")
with tempfile.TemporaryDirectory() as folder:
    path=Path(folder)/"onepass.log"
    # Split a serial reply in the middle of the checksum and concatenate
    # across separately timestamped RX lines. Normal TX lines must be ignored.
    log=["2026-10-24T00:01:02 TX READ_D0 5A-A5-00-3D-20-01-D0-00-00"]
    for k in range(0,len(raw),7):
        chunk=raw[k:k+7]
        log.append("2026-10-24T00:01:02 RX "+chunk.hex("-").upper())
    path.write_text("\n".join(log)+"\n")
    report=make_report(path)
    assert report["valid_frames"]==5
    assert report["checksum_failures_encountered"]==0
    assert report["read_ack_registers"]["FE"][0]["phase_currents_raw_counts"]==[-5,7,-2]
print("PASS G30 offline snapshot decoder: fragmented RX, checksum, D0/D9/FD/FE/FF and signed currents")
