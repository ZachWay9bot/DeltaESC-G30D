#!/usr/bin/env python3
"""Read-only model of observed stock G30D IAP error branches.

Bootloader 0x080006D8 returns 2 for FLASH erase failure.
Bootloader 0x080005FC checks result 2; path 0x08000604 clears pending.
Bootloader 0x080006A6 returns 3 for programming/verify failure;
that result leaves pending set (0x08000622).
The optional owner-original dump is never sent or uploaded.
"""
import argparse
from pathlib import Path

PAGE=1024
CLEAR_PENDING_ON_ERROR={1,2,6}

def verify_dump(p):
    b=Path(p).read_bytes()
    assert len(b)==131072
    assert b[0x6e4:0x6f0].hex()=='2046fff72ffe00b9022662b6'
    assert b[0x5fc:0x60c].hex()=='1148006802280ed100200e49486000f0'
    assert b[0x6a4:0x6aa].hex()=='08b90320d8e7'
    print('Owner-original bootloader branch signatures: PASS')

def returned_erase_error_counterexample():
    # First app flash page is already erased, next page erase reports failure.
    original=bytes.fromhex('0050002001110008')+bytes([0x21])*(3*PAGE-8)
    active=bytearray(original)
    active[:PAGE]=bytes([0xff])*PAGE
    pending=True
    result=2
    if result in CLEAR_PENDING_ON_ERROR: pending=False
    assert not pending
    assert active[:8]==bytes([0xff])*8
    print('EXPECTED UNSAFE CASE: erase failure=2 clears pending with invalid app vector')

def returned_write_error():
    pending=True
    result=3
    if result in CLEAR_PENDING_ON_ERROR: pending=False
    assert pending
    print('Write verification failure=3 leaves pending for a later retry: PASS')

def main():
    ap=argparse.ArgumentParser()
    ap.add_argument('--dump',type=Path,help='private local 128-KiB stock dump')
    a=ap.parse_args()
    if a.dump: verify_dump(a.dump)
    returned_write_error()
    returned_erase_error_counterexample()
    print('LIMIT: these are discrete returned-operation/error paths, not physical brownout tests')

if __name__=='__main__':main()
