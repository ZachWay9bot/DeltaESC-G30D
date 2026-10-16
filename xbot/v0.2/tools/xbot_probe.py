#!/usr/bin/env python3
import argparse,struct,time
try: import serial
except ImportError: raise SystemExit('Install pyserial: py -m pip install pyserial')
def frame(cmd,index=0,data=b'',src=0x3D,dst=0x20):
 h=bytes([len(data),src,dst,cmd,index])+data;c=(~sum(h))&0xffff;return b'\x5a\xa5'+h+struct.pack('<H',c)
def recv(ser,timeout=1.0):
 end=time.time()+timeout;buf=bytearray()
 while time.time()<end:
  x=ser.read(1)
  if not x: continue
  buf+=x
  while len(buf)>=2 and buf[:2]!=b'\x5a\xa5': del buf[0]
  if len(buf)>=3:
   n=buf[2]; total=n+9
   if len(buf)>=total:
    r=bytes(buf[:total]);del buf[:total]
    h=r[2:-2]; got=struct.unpack('<H',r[-2:])[0]
    if ((~sum(h))&0xffff)==got:return r
 raise TimeoutError('no valid reply')
def u16(d,o):return struct.unpack_from('<H',d,o)[0]
def u32(d,o):return struct.unpack_from('<I',d,o)[0]
def main():
 ap=argparse.ArgumentParser();ap.add_argument('port');ap.add_argument('--rate',type=float,default=5);a=ap.parse_args()
 with serial.Serial(a.port,115200,timeout=.05) as s:
  while True:
   s.write(frame(0xF0));r=recv(s);n=r[2];cmd=r[5];d=r[7:7+n]
   if cmd!=0xF0 or len(d)!=44:print('unexpected',r.hex());continue
   direction=d[27] if d[27]<128 else d[27]-256
   print(f"ms={u32(d,4):8d} VbatADC={u16(d,8):4d} I={u16(d,10):4d}/{u16(d,12):4d}/{u16(d,14):4d} BEMF={u16(d,16):4d}/{u16(d,18):4d}/{u16(d,20):4d} cm={u16(d,22):4d} span={u16(d,24):4d} sec={d[26]:3d} dir={direction:+d} trans={u32(d,28)} bad={u32(d,32)} eHz={u32(d,36)/10:.1f} rpm={u32(d,40)/10:.1f}")
   time.sleep(max(.02,1/a.rate))
if __name__=='__main__':main()
