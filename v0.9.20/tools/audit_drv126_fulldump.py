#!/usr/bin/env python3
"""Read-only structural audit for a 128 KiB Ninebot G30D STM32F103 full dump.

This script reports firmware/layout/peripheral evidence only. Literal addresses
inside a binary do NOT prove PCB routing, analog gain, gate-driver polarity or
safe motor operation.
"""
from pathlib import Path
import argparse, hashlib, struct

FLASH_BASE=0x08000000
FLASH_SIZE=128*1024
SRAM_LO=0x20000000
SRAM_HI=0x20005000

PERIPHERALS={
    'TIM1':0x40012C00,'ADC1':0x40012400,'ADC2':0x40012800,
    'GPIOA':0x40010800,'GPIOB':0x40010C00,'GPIOC':0x40011000,
    'AFIO':0x40010000,'RCC':0x40021000,
    'USART1':0x40013800,'USART2':0x40004400,'USART3':0x40004800,
    'DMA1':0x40020000,'FLASH':0x40022000,'SCB_VTOR':0xE000ED08,
}

def u32(b,o):
    return struct.unpack_from('<I',b,o)[0]

def vector_ok(sp,reset):
    return SRAM_LO <= sp < SRAM_HI and (reset & 1) and FLASH_BASE <= (reset & ~1) < FLASH_BASE+FLASH_SIZE

def literal_count(b,value):
    needle=struct.pack('<I',value)
    return sum(1 for i in range(0,len(b)-3,2) if b[i:i+4]==needle)

def main():
    ap=argparse.ArgumentParser()
    ap.add_argument('dump',type=Path)
    a=ap.parse_args()
    b=a.dump.read_bytes()
    if len(b)!=FLASH_SIZE:
        raise SystemExit(f'FAIL: expected {FLASH_SIZE} bytes, got {len(b)}')

    print('size_bytes=',len(b),sep='')
    print('sha256=',hashlib.sha256(b).hexdigest(),sep='')
    for off,name in ((0,'boot'),(0x1000,'app')):
        sp,reset=u32(b,off),u32(b,off+4)
        print(f'{name}_sp=0x{sp:08X}')
        print(f'{name}_reset=0x{reset:08X}')
        print(f'{name}_vector_valid={int(vector_ok(sp,reset))}')

    for start,end,name in ((0xD800,0xE000,'d800_dfff'),
                           (0xE000,0xF000,'e000_efff')):
        print(f'{name}_all_ff={int(all(x==0xFF for x in b[start:end]))}')

    for name,address in PERIPHERALS.items():
        print(f'literal_{name}={literal_count(b,address)}')

    if not vector_ok(u32(b,0),u32(b,4)):
        raise SystemExit('FAIL: invalid boot vector')
    if not vector_ok(u32(b,0x1000),u32(b,0x1004)):
        raise SystemExit('FAIL: invalid app vector at 0x08001000')

    print('NOTE=static firmware evidence only; not electrical pin/gain/safety proof')

if __name__=='__main__':
    main()
