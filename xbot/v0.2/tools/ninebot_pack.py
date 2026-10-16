#!/usr/bin/env python3
import argparse,struct,sys
KEY0=bytes.fromhex('FE801CB2D1EF41A6A41731F5A06824F0')
DELTA=0x9E3779B9
MASK=0xffffffff
def mutate(k): return bytes((b+i)&0xff for i,b in enumerate(k))
def tea_enc(block,key):
 y,z=struct.unpack('<II',block); k=struct.unpack('<IIII',key); s=0
 for _ in range(32):
  s=(s+DELTA)&MASK
  y=(y+((((z<<4)&MASK)+k[0])^((z+s)&MASK)^((z>>5)+k[1])))&MASK
  z=(z+((((y<<4)&MASK)+k[2])^((y+s)&MASK)^((y>>5)+k[3])))&MASK
 return struct.pack('<II',y,z)
def tea_dec(block,key):
 y,z=struct.unpack('<II',block); k=struct.unpack('<IIII',key); s=0xC6EF3720
 for _ in range(32):
  z=(z-((((y<<4)&MASK)+k[2])^((y+s)&MASK)^((y>>5)+k[3])))&MASK
  y=(y-((((z<<4)&MASK)+k[0])^((z+s)&MASK)^((z>>5)+k[1])))&MASK
  s=(s-DELTA)&MASK
 return struct.pack('<II',y,z)
def crypt(data,decrypt=False):
 assert len(data)%8==0
 key=KEY0; iv=b'\0'*8; out=bytearray(); off=0
 for i in range(0,len(data),8):
  b=data[i:i+8]
  if decrypt:
   t=tea_dec(b,key); p=bytes(x^y for x,y in zip(t,iv)); out+=p; iv=b
  else:
   x=bytes(a^b for a,b in zip(b,iv)); c=tea_enc(x,key); out+=c; iv=c
  off+=8
  if off%1024==0:key=mutate(key)
 return bytes(out)
def checksum(body):
 if len(body)%4: raise ValueError('body must be word aligned')
 s=sum(struct.unpack('<%dI'%(len(body)//4),body))&MASK
 r=((s<<16)|(s>>16))&MASK
 return (~r)&MASK
def make_plain(raw):
 body=bytearray(raw)
 while len(body)%8!=4: body.append(0xff)
 return bytes(body)+struct.pack('<I',checksum(body))
def validate_plain(p):
 if len(p)<16 or len(p)%8: return False
 if len(p[:-4])%4:return False
 return struct.unpack('<I',p[-4:])[0]==checksum(p[:-4])
def main():
 ap=argparse.ArgumentParser(); sub=ap.add_subparsers(dest='cmd',required=True)
 p=sub.add_parser('pack');p.add_argument('input');p.add_argument('output')
 p=sub.add_parser('decrypt');p.add_argument('input');p.add_argument('output')
 p=sub.add_parser('verify');p.add_argument('input')
 a=ap.parse_args(); d=open(a.input,'rb').read()
 if a.cmd=='pack':
  plain=make_plain(d)
  if len(plain)>0xD800: raise SystemExit('package exceeds 0x0800E800..0x0801BFFF staging window')
  enc=crypt(plain,False); open(a.output,'wb').write(enc); print(f'raw={len(d)} plain={len(plain)} enc={len(enc)} checksum={checksum(plain[:-4]):08x}')
 elif a.cmd=='decrypt':
  p=crypt(d,True);open(a.output,'wb').write(p);print('valid',validate_plain(p),'len',len(p))
 else:
  p=crypt(d,True);print('valid',validate_plain(p),'len',len(p),'sp/reset',p[:8].hex());sys.exit(0 if validate_plain(p) else 1)
if __name__=='__main__':main()
