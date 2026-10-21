#!/usr/bin/env python3
from __future__ import annotations
import argparse, hashlib, json, struct, sys, zipfile
from pathlib import Path

APP_BASE=0x08001000
APP_LIMIT=0x0800E000
MAX_APP_SIZE=APP_LIMIT-APP_BASE
DEFAULT_KEY=bytes([0xFE,0x80,0x1C,0xB2,0xD1,0xEF,0x41,0xA6,0xA4,0x17,0x31,0xF5,0xA0,0x68,0x24,0xF0])
DELTA=0x9E3779B9
MASK32=0xFFFFFFFF

def _u32(v): return v & MASK32

def _key_words(key):
    if len(key)!=16: raise ValueError('NinebotTEA key must be 16 bytes')
    return list(struct.unpack('<4I',key))

def _update_key(words):
    raw=bytearray(struct.pack('<4I',*words))
    for i in range(16): raw[i]=(raw[i]+i)&0xFF
    return list(struct.unpack('<4I',raw))

def _encrypt_block(y,z,key):
    total=0
    for _ in range(32):
        total=_u32(total+DELTA)
        y=_u32(y+(((_u32(z<<4)+key[0])^(z+total)^((z>>5)+key[1]))))
        z=_u32(z+(((_u32(y<<4)+key[2])^(y+total)^((y>>5)+key[3]))))
    return y,z

def _checksum(body):
    if len(body)%4: raise ValueError('checksum body must be 32-bit aligned')
    total=0
    for off in range(0,len(body),4): total=_u32(total+struct.unpack_from('<I',body,off)[0])
    total=((total>>16)&0xFFFF)|((total&0xFFFF)<<16)
    return _u32(total^MASK32)

def ninebot_tea_encrypt(plain,key=DEFAULT_KEY):
    body=bytearray(plain)
    while len(body)%4: body.append(0)
    if len(body)%8==0: body.extend(b'\0'*4)
    body.extend(struct.pack('<I',_checksum(body)))
    if len(body)%8: raise AssertionError('NinebotTEA padded size is not 8-byte aligned')
    out=bytearray(len(body)); kw=_key_words(key); iv_lo=iv_hi=0; processed=0
    for off in range(0,len(body),8):
        if processed==1024: kw=_update_key(kw); processed=0
        y,z=struct.unpack_from('<II',body,off); y^=iv_lo; z^=iv_hi
        ey,ez=_encrypt_block(y,z,kw); struct.pack_into('<II',out,off,ey,ez)
        iv_lo,iv_hi=ey,ez; processed+=8
    return bytes(out)

def validate_app_image(data):
    if len(data)<8: raise ValueError('firmware image too short')
    if len(data)>MAX_APP_SIZE: raise ValueError(f'image {len(data)} > {MAX_APP_SIZE} bytes')
    sp,reset=struct.unpack_from('<II',data,0); ra=reset&~1
    if not 0x20000000<=sp<=0x20005000: raise ValueError(f'invalid SP 0x{sp:08X}')
    if not (reset&1): raise ValueError(f'reset not Thumb 0x{reset:08X}')
    if not APP_BASE<=ra<APP_LIMIT: raise ValueError(f'reset outside app 0x{reset:08X}')
    return sp,reset

def md5hex(data): return hashlib.md5(data).hexdigest()

def build_package(firmware:Path,output:Path,name:str,variant:str):
    plain=firmware.read_bytes(); sp,reset=validate_app_image(plain); enc=ninebot_tea_encrypt(plain)
    info={
      'schemaVersion':1,
      'firmware':{
        'displayName':name,'model':'max','enforceModel':True,'type':'DRV',
        'compatible':['max_DRV_STM32F103CxT6'],'encryption':'encrypted',
        'md5':{'enc':md5hex(enc)}
      }
    }
    notes=(
      f'DeltaESC G30D v0.6.7 SHU staged-IAP candidate ({variant})\n'
      'Application base: 0x08001000; stock 4 KiB bootloader preserved\n'
      'App limit: 0x0800E000 (52 KiB)\n'
      'Reflash: stock-compatible 5A A5 IAP 07/08/09/0A -> decrypt/write staging @ 0x0800E800 -> validate -> control @ 0x0801F800 -> reset -> preserved stock bootloader copies image\n'
      'WARNING: staged-IAP reflash/rollback remains hardware-unvalidated on this controller; ST-Link is recovery-only, not the normal update path.\n'
      f'Image size: {len(plain)} bytes\nInitial SP: 0x{sp:08X}\nReset handler: 0x{reset:08X}\n'
    )
    output.parent.mkdir(parents=True,exist_ok=True)
    with zipfile.ZipFile(output,'w',compression=zipfile.ZIP_DEFLATED) as z:
        z.writestr('FIRM.bin.enc',enc)
        z.writestr('info.json',json.dumps(info,indent=4)+'\n'); z.writestr('params.txt',notes)
    return plain,enc

def main():
    ap=argparse.ArgumentParser(); ap.add_argument('firmware',type=Path); ap.add_argument('output',type=Path)
    ap.add_argument('--name',default='DeltaESC G30D v0.6.7'); ap.add_argument('--variant',default='syncsafe')
    a=ap.parse_args(); plain,enc=build_package(a.firmware,a.output,a.name,a.variant)
    print(f'Wrote {a.output}'); print(f'FIRM.bin {len(plain)} md5={md5hex(plain)}'); print(f'FIRM.bin.enc {len(enc)} md5={md5hex(enc)}')
    return 0
if __name__=='__main__':
    try: raise SystemExit(main())
    except (OSError,ValueError) as e: print(f'ERROR: {e}',file=sys.stderr); raise SystemExit(2)
