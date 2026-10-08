"""Run complete SYNC_SAFE ARM firmware against synthetic MMIO, not physical BLE.

UART bytes traverse the real IRQ, parser, main loop, replies and flash updater.
No analogue behavior, flash latency, interrupt timing or real bootloader is modeled.
"""
import argparse, json, struct, subprocess, sys, os
from pathlib import Path
from unicorn import Uc, UC_ARCH_ARM, UC_MODE_THUMB, UC_MODE_MCLASS
from unicorn import UC_HOOK_CODE, UC_HOOK_MEM_READ, UC_HOOK_MEM_WRITE
from unicorn.arm_const import UC_ARM_REG_SP, UC_ARM_REG_LR, UC_ARM_REG_PC, UC_ARM_REG_XPSR

BASE = Path(__file__).resolve().parent
PREFIX = BASE.parent / 'toolchain/usr/bin/arm-none-eabi-'

def frame(cmd, payload=b'', arg=0, src=0x3e, dst=0x20):
    body=bytes([len(payload),src,dst,cmd,arg])+payload
    return b'\x5a\xa5'+body+struct.pack('<H',sum(body)^0xffff)

class Target:
    def __init__(self, root):
        self.root=root
        self.sym={}
        for row in subprocess.check_output([os.environ.get('ARM_NM',str(PREFIX)+'nm'),'-n',str(root/'build/target_syncsafe.elf')],text=True).splitlines():
            fields=row.split()
            if len(fields)==3:self.sym[fields[2]]=int(fields[0],16)
        binary=(root/'build/target_syncsafe.bin').read_bytes()
        self.u=Uc(UC_ARCH_ARM,UC_MODE_THUMB|UC_MODE_MCLASS)
        self.u.mem_map(0x08000000,0x20000);self.u.mem_write(0x08000000,b'\xff'*0x20000)
        self.u.mem_write(0x08001000,binary)
        self.u.mem_map(0x20000000,0x5000);self.u.mem_map(0x09000000,0x1000)
        self.u.mem_map(0x40000000,0x30000);self.u.mem_map(0xe0000000,0x100000)
        self.tx=bytearray();self.rx=None;self.stop=None;self.resume=None
        self.erases=[];self.programs=[];self.reset=False
        self.u.hook_add(UC_HOOK_CODE,self.code)
        for lo,hi in [(0x40000000,0x4002ffff),(0xe0000000,0xe00fffff)]:
            self.u.hook_add(UC_HOOK_MEM_READ,self.read,begin=lo,end=hi)
            self.u.hook_add(UC_HOOK_MEM_WRITE,self.write,begin=lo,end=hi)
        self.u.hook_add(UC_HOOK_MEM_WRITE,self.write,begin=0x08000000,end=0x0801ffff)
        self.u.reg_write(UC_ARM_REG_SP,struct.unpack_from('<I',binary)[0])
        self.u.emu_start(struct.unpack_from('<I',binary,4)[0],0,count=1500000)
        assert self.stop=='wfi',('boot stalled',hex(self.u.reg_read(UC_ARM_REG_PC)))
        assert self.r32(0xe000ed08)==0x08001000,'VTOR not relocated'
    def r32(self,a):return int.from_bytes(self.u.mem_read(a,4),'little')
    def w32(self,a,v):self.u.mem_write(a,struct.pack('<I',v&0xffffffff))
    def code(self,u,a,size,_):
        if a==0x09000000:self.stop='return';u.emu_stop()
        elif a==self.sym['Default_Handler']:raise AssertionError('target exception')
        elif size==2 and bytes(u.mem_read(a,2))==b'\x30\xbf':
            self.stop='wfi';self.resume=a+2;u.emu_stop()
    def read(self,u,access,a,size,value,_):
        if a==0x40021000:
            v=self.r32(a);self.w32(a,v|2|((v&(1<<24))<<1))
        elif a==0x40021004:
            v=self.r32(a);self.w32(a,(v&~12)|((v&3)<<2))
        elif a==0x40012408:self.w32(a,self.r32(a)&~12)
        elif a in (0x40004400,0x40013800):self.w32(a,0xc0|(0x20 if a==0x40004400 and self.rx is not None else 0))
        elif a==0x40004404 and self.rx is not None:self.w32(a,self.rx);self.rx=None
        elif a==0xe0001004:self.w32(a,self.r32(a)+100)
        elif a==0x4002200c:self.w32(a,0)
        elif a==0x40022010:self.w32(a,self.r32(a)&~0x40) # erase START clears in hardware
    def write(self,u,access,a,size,value,_):
        if a==0x40004404:self.tx.append(value&255)
        # MOE is used for TIM1 CH4 ADC timing; gate channel enables must stay zero.
        if a==0x40012c44 and value&0x8000:assert not self.r32(0x40012c20)&0x555,'SYNC_SAFE enabled bridge'
        if a==0x40012c20:assert not value&0x555,'SYNC_SAFE enabled gate channels'
        if a==0x40022004 and value==0xcdef89ab:self.w32(0x40022010,self.r32(0x40022010)&~0x80)
        if a==0x40022010 and value&0x40:
            page=self.r32(0x40022014)
            assert page%1024==0 and (0x0800e800<=page<0x0801c000 or page==0x0801f800)
            self.erases.append(page);u.mem_write(page,b'\xff'*1024)
        if 0x08000000<=a<0x08020000:
            assert size==2 and (0x0800e800<=a<0x0801c000 or 0x0801f800<=a<0x0801fc00)
            old=int.from_bytes(u.mem_read(a,size),'little');assert old&value==value,'flash 0 -> 1'
            assert self.r32(0x40022010)&1
            self.programs.append((a,value))
        if a==0xe000ed0c and value&4:self.reset=True;self.stop='reset';u.emu_stop()
    def irq(self):
        ctx=self.u.context_save();self.stop=None
        self.u.reg_write(UC_ARM_REG_SP,self.u.reg_read(UC_ARM_REG_SP)-128)
        self.u.reg_write(UC_ARM_REG_LR,0x09000001);self.u.reg_write(UC_ARM_REG_XPSR,0x01000000)
        self.u.emu_start(self.sym['USART2_IRQHandler']|1,0,count=200000)
        assert self.stop=='return','UART IRQ did not return';self.u.context_restore(ctx)
    def exchange(self,raw):
        start=len(self.tx)
        for b in raw:self.rx=b;self.irq()
        self.stop=None;self.u.emu_start(self.resume|1,0,count=3000000)
        assert self.stop in ('wfi','reset'),('main loop stalled',hex(self.u.reg_read(UC_ARM_REG_PC)))
        return bytes(self.tx[start:])
    def packet(self,*a,**kw):
        out=self.exchange(frame(*a,**kw))
        if out:
            assert out[:2]==b'\x5a\xa5' and len(out)==out[2]+9,out.hex()
            assert int.from_bytes(out[-2:],'little')==sum(out[2:-2])^0xffff
        return out

def main():
    ap=argparse.ArgumentParser();ap.add_argument('root',type=Path);ap.add_argument('--retry',action='store_true');ap.add_argument('--guard',action='store_true');ap.add_argument('--blocks',type=int,default=5);args=ap.parse_args()
    t=Target(args.root.resolve());captures={}
    for reg,n in [(0x1a,2),(0x10,14),(0xd0,16),(0xd1,14),(0xd2,5),(0xd3,4),(0xd4,4),(0xd5,4),(0xd6,14),(0xd7,8),(0xd8,12),(0xd9,6)]:
        out=t.packet(1,bytes([n]),arg=reg);assert out[2]==n and out[3:7]==bytes([0x20,0x3e,4,reg]),out.hex()
        captures[f'{reg:02x}']=out.hex()
    assert bytes.fromhex(captures['d0'])[7:11]==b'DESC'
    assert bytes.fromhex(captures['d0'])[16]&7==0,'motor permitted/armed in SAFE identity'
    out=t.packet(3,b'\xde\xc0\xf4\x01',arg=0xe5);assert out[7]==4,'SAFE accepted arming'
    out=t.packet(3,b'',arg=0xe6);assert out[7]==0
    out=t.packet(0x64,b'',src=0x21);assert out[2]==6 and out[3:7]==b'\x20\x21\x64\x00'
    corrupt=bytearray(frame(1,b'\x10',arg=0xd0));corrupt[-1]^=1;assert t.exchange(corrupt)==b''
    assert t.packet(1,b'\x10',arg=0xd0)[7:11]==b'DESC'
    print('PASS actual ARM: app reads, checksum rejection/recovery, dashboard response, SAFE arm refusal')
    t=Target(args.root.resolve())
    # Synthetic encrypted image exercises key rollover and final padded transport.
    sys.path.insert(0,str(args.root.resolve()/'tools'))
    from make_shu_zip import ninebot_tea_encrypt
    plain=struct.pack('<II',0x20005000,0x08001081)+bytes(range(256))*args.blocks
    enc=ninebot_tea_encrypt(plain)
    out=t.packet(7,struct.pack('<I',len(enc)));assert out[5:7]==b'\x0b\x00',out.hex()
    for i,off in enumerate(range(0,len(enc),128)):
        p=enc[off:off+128].ljust(128,b'\0')
        out=t.packet(8,p,arg=i&255);assert out[5:7]==b'\x0b\x00',(i,out.hex())
        if args.retry:
            before=list(t.programs);out=t.packet(8,p,arg=i&255)
            assert out[5:7]==b'\x0b\x00',('lost-ACK retry failed',i,out.hex())
            assert t.programs==before,'identical retry reprogrammed flash'
    out=t.packet(9,struct.pack('<I',~sum(enc)&0xffffffff));assert out[5:7]==b'\x0b\x00',out.hex()
    if args.retry:
        before=list(t.programs);out=t.packet(9,struct.pack('<I',~sum(enc)&0xffffffff))
        assert out[5:7]==b'\x0b\x00' and t.programs==before,'CRC ACK retry failed or reprogrammed control'
    assert bytes(t.u.mem_read(0x0800e800,len(plain)))==plain,'decrypted staging differs'
    magic,pending,n=struct.unpack('<III',bytes(t.u.mem_read(0x0801f800,12)))
    assert magic==0x505a and pending==1 and n>=len(plain)
    if args.guard:
        assert t.erases[0]==0x0801f800,'old control was not invalidated first'
        assert t.programs[-1]==(0x0801f800,0x505a),'magic not committed last'
    out=t.packet(10);assert out[5:7]==b'\x0b\x00' and t.reset
    print('PASS actual ARM: BEGIN, 128-byte encrypted WR, key rollover, padding, CRC, staging bytes, commit, RESET')
    if args.retry:
        t=Target(args.root.resolve());assert t.packet(7,struct.pack('<I',len(enc)))[6]==0
        first=enc[:128];assert t.packet(8,first,arg=0)[6]==0
        before=list(t.programs);changed=bytearray(first);changed[-1]^=1
        out=t.packet(8,bytes(changed),arg=0)
        assert out[6]==4 and t.programs==before,'changed duplicate accepted or programmed'
        t=Target(args.root.resolve());assert t.packet(7,struct.pack('<I',len(enc)))[6]==0
        assert t.packet(8,enc[:128],arg=2)[6]==4 and not t.programs,'out-of-order packet programmed'
        t=Target(args.root.resolve());before=list(t.erases)
        assert t.packet(7,struct.pack('<I',0x100000))[6]==2 and t.erases==before,'invalid size erased flash'
        print('PASS actual ARM: changed duplicate / out-of-order / oversized BEGIN refused without programming')
    (args.root/'build/full_target_captures.json').write_text(json.dumps(captures,indent=2)+'\n')

if __name__=='__main__':main()
