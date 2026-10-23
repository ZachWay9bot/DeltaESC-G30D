#!/usr/bin/env python3
import struct
from pathlib import Path
from make_shu_zip import ninebot_tea_encrypt
KEY=bytes([0xFE,0x80,0x1C,0xB2,0xD1,0xEF,0x41,0xA6,0xA4,0x17,0x31,0xF5,0xA0,0x68,0x24,0xF0])
MASK=0xffffffff; DELTA=0x9E3779B9
KNOWN_DRV126_HEAD=bytes.fromhex('f44cbd816b2d4231af4ed3ca9305484e268a121aa920ef7d874db17bae516b69')

def words(k): return list(struct.unpack('<4I',k))
def update(k):
    b=bytearray(struct.pack('<4I',*k))
    for i in range(16): b[i]=(b[i]+i)&255
    return list(struct.unpack('<4I',b))
def decblock(y,z,k):
    s=0xC6EF3720
    for _ in range(32):
        z=(z-((((y<<4)&MASK)+k[2])^(y+s)^((y>>5)+k[3])))&MASK
        y=(y-((((z<<4)&MASK)+k[0])^(z+s)^((z>>5)+k[1])))&MASK
        s=(s-DELTA)&MASK
    return y,z
def decrypt(enc):
    assert len(enc)%8==0
    out=bytearray(); k=words(KEY); iv0=iv1=0; processed=0
    for o in range(0,len(enc),8):
        if processed==1024: k=update(k); processed=0
        cy,cz=struct.unpack_from('<II',enc,o); y,z=decblock(cy,cz,k); y^=iv0; z^=iv1
        out+=struct.pack('<II',y,z);iv0,iv1=cy,cz;processed+=8
    return bytes(out)
def cksum(body):
    assert len(body)%4==0
    s=sum(struct.unpack_from('<I',body,o)[0] for o in range(0,len(body),4))&MASK
    return ((((s>>16)&0xffff)|((s&0xffff)<<16))^MASK)&MASK

# Independent known official DRV126 encrypted prefix.
plain_head=bytes.fromhex('182b002001110008ed450008053c00089d4500089d1c00083d7d000800000000')
# Encryption routine must reproduce the official first 32 bytes.
assert ninebot_tea_encrypt(plain_head)[:32] == KNOWN_DRV126_HEAD
# Round-trip a real v0.6.2 image and validate the internal trailing checksum.
p=next(Path('build').glob('*syncsafe.bin')).read_bytes(); e=ninebot_tea_encrypt(p); d=decrypt(e)
assert d[:len(p)]==p
assert struct.unpack_from('<I',d,len(d)-4)[0]==cksum(d[:-4])
print('IAP/NinebotTEA regression: PASS',len(p),len(e))
