#!/usr/bin/env python3
"""Read-only, discrete-operation model of stock G30D IAP copy.

Based on disassembly of the user's DRV126 bootloader:
0x0800064C: erase active image, copy and verify in 0x100-byte chunks.
0x080005C8: clear pending update only after successful copy.
No real flash/MCU writes. No claim of recovery from power loss INSIDE erase/program.
"""
import argparse
import hashlib
import struct
from pathlib import Path

BOOT=0x08000000
APP=0x08001000
STAGE=0x0800E800
CTRL=0x0801F800
CTRL_SECOND=0x0801FC00
PAGE=1024
BLOCK=256
MAX_APP=0xC800

def roundup(v,step):
    return (v+step-1)//step*step

def check_layout(size):
    assert 256<=size<=MAX_APP
    erase_end=APP+roundup(size+128,PAGE)
    written_end=APP+roundup(size,BLOCK)
    staged_end=STAGE+size
    assert written_end<=erase_end<=STAGE, (size,'copy/erase may overlap stage')
    assert staged_end<=CTRL, (size,'stage overlaps control')
    return erase_end,written_end

def retry_model(size,cut):
    image=bytes((i*17+(i>>5)*3)&0xff for i in range(size))
    padded=image.ljust(roundup(size,BLOCK),b'\xff')
    active=bytearray(b'\xa5'*roundup(size+128,PAGE))
    pending=True
    blocks=roundup(size,BLOCK)//BLOCK
    for attempt in range(2):
        if not pending: break
        erase_end,_=check_layout(size)
        active[:erase_end-APP]=b'\xff'*(erase_end-APP)
        for idx in range(blocks):
            if attempt==0 and idx==cut:
                # The staged image + valid control marker survive reset.
                break
            off=idx*BLOCK
            active[off:off+BLOCK]=padded[off:off+BLOCK]
        else:
            pending=False
    assert not pending and active[:size]==image
    return blocks

def check_private_dump(path):
    b=Path(path).read_bytes()
    assert len(b)==131072
    print('Local dump SHA256:',hashlib.sha256(b).hexdigest())
    for addr in (CTRL,CTRL_SECOND):
        off=addr-BOOT
        assert struct.unpack_from('<III',b,off)==(0x505A,0,0)
        assert b[off+12:off+1024]==bytes([0xff])*1012
    boot=b[:4096]
    for addr in (APP,STAGE,CTRL,CTRL_SECOND):
        needle=struct.pack('<I',addr)
        hits=[i for i in range(0,len(b)-3,2) if b[i:i+4]==needle]
        print(f'Exact literal 0x{addr:08X}:',len(hits),'hits')
    assert boot.count(struct.pack('<I',CTRL))>=2
    assert boot.count(struct.pack('<I',STAGE))>=1
    assert b.count(struct.pack('<I',CTRL_SECOND))==0
    print('Local dump layout: PASS (indirect second-slot use not excluded)')

def main():
    p=argparse.ArgumentParser()
    p.add_argument('--dump',type=Path,help='private local 128 KiB stock dump; NEVER upload it')
    args=p.parse_args()
    count=0
    for size in range(256,MAX_APP+1,8):
        check_layout(size)
        count+=1
    cuts=0
    for size in (256,10240,10484,10712,32768,48864,MAX_APP):
        for cut in range(roundup(size,BLOCK)//BLOCK+1):
            retry_model(size,cut)
            cuts+=1
    print('Bootloader erase/copy layout:',count,'size checks PASS')
    print('Discrete-operation interruption/retry:',cuts,'cases PASS')
    print('LIMIT: excludes partially interrupted physical erase/program, damaged stage/control, and hardware power behavior')
    if args.dump: check_private_dump(args.dump)

if __name__=='__main__':
    main()
