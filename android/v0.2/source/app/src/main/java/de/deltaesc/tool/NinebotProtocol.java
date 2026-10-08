package de.deltaesc.tool;

import java.util.Arrays;

final class NinebotProtocol {
    static final int PHONE = 0x3E, ESC = 0x20, BLE = 0x21;
    static final int READ = 0x01, WRITE = 0x02, READ_ACK = 0x04;
    static final byte[] APP_KEY = Hex.parse("4A EE BD 73 E2 16 1C 11 2D 06 5A 49 CC 6E 8B B7");

    static final class Frame {
        final int address,dst,type,command; final byte[] payload,raw;
        Frame(int src,int dst,int type,int arg,byte[] payload,byte[] raw){
            this.address=src;this.dst=dst;this.type=type;this.command=arg;this.payload=payload;this.raw=raw;
        }
    }
    static byte[] build(int dst,int cmd,int arg,byte[] payload){
        if(payload==null) payload=new byte[0];
        if(payload.length>255) throw new IllegalArgumentException("Payload zu lang");
        byte[] out=new byte[payload.length+7];
        out[0]=0x5A;out[1]=(byte)0xA5;out[2]=(byte)payload.length;
        out[3]=(byte)PHONE;out[4]=(byte)dst;out[5]=(byte)cmd;out[6]=(byte)arg;
        System.arraycopy(payload,0,out,7,payload.length); return out;
    }
    static byte[] read(int reg,int requested){ return build(ESC,READ,reg,new byte[]{(byte)requested}); }
    static byte[] write(int reg,byte[] payload){ return build(ESC,WRITE,reg,payload); }
    static byte[] init(){ return build(BLE,0x5B,0,new byte[0]); }
    static byte[] setKey(){ return build(BLE,0x5C,0,APP_KEY); }
    static byte[] pair(byte[] serial){
        if(serial==null||serial.length!=14) throw new IllegalArgumentException("14-byte serial required");
        return build(BLE,0x5D,0,serial);
    }
    static boolean valid(byte[] f){
        return f!=null&&f.length>=7&&f[0]==0x5A&&(f[1]&0xFF)==0xA5&&f.length==(f[2]&0xFF)+7;
    }
    static Frame decode(byte[] raw){
        if(!valid(raw)) throw new IllegalArgumentException("Ungültiger 5A A5 Frame");
        int n=raw[2]&0xFF;
        return new Frame(raw[3]&0xFF,raw[4]&0xFF,raw[5]&0xFF,raw[6]&0xFF,
                n==0?new byte[0]:Arrays.copyOfRange(raw,7,7+n),raw);
    }
    static boolean isEscWrite(byte[] raw){ return valid(raw)&&(raw[4]&0xFF)==ESC&&(raw[5]&0xFF)==WRITE; }
}
