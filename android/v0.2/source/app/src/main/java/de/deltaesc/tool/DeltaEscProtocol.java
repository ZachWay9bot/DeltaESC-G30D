package de.deltaesc.tool;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.util.Locale;

final class DeltaEscProtocol {
    private DeltaEscProtocol() {}
    static final int EXPECTED_BUILD = 0x0603;
    static final int REG_STATUS=0xD0,REG_ADC=0xD1,REG_PHASE=0xD2,REG_R=0xD3,REG_L=0xD4,REG_FLUX=0xD5,REG_OBSERVER=0xD6,REG_LOCK=0xD7,REG_TIMING=0xD8,REG_FAULT=0xD9;
    static final int ACT_CAL_OFFSET=0xE0,ACT_CAL_PHASE=0xE1,ACT_CAL_R=0xE2,ACT_CAL_L=0xE3,ACT_CAL_FLUX=0xE4,ACT_START_TEST=0xE5,ACT_STOP=0xE6;
    static final int CFG_R_UOHM=0xF0,CFG_L_NH=0xF1,CFG_FLUX_UWB=0xF2,CFG_PHASE_OFFSET=0xF3,CFG_TEST_CURRENT_MA=0xF4,CFG_COMMIT=0xF5;
    static final int MAGIC=0xC0DE;
    static byte[] action(int command){if(command==ACT_STOP)return NinebotProtocol.write(command,new byte[0]);return NinebotProtocol.write(command,le16(MAGIC));}
    static byte[] startTest(int currentMa){ByteBuffer b=ByteBuffer.allocate(4).order(ByteOrder.LITTLE_ENDIAN);b.putShort((short)MAGIC);b.putShort((short)clamp(currentMa,0,10000));return NinebotProtocol.write(ACT_START_TEST,b.array());}
    static byte[] writeU32(int command,long value){ByteBuffer b=ByteBuffer.allocate(4).order(ByteOrder.LITTLE_ENDIAN);b.putInt((int)(value&0xFFFFFFFFL));return NinebotProtocol.write(command,b.array());}
    static byte[] writeI16(int command,int value){return NinebotProtocol.write(command,le16(value));}
    static byte[] commit(){return NinebotProtocol.write(CFG_COMMIT,le16(MAGIC));}
    static String describe(NinebotProtocol.Frame f){
        byte[] p=f.payload;
        try{
            switch(f.command){
                case REG_STATUS:
                    if(isDeltaEscIdentity(f))return String.format(Locale.US,"DeltaESC DESC proto %d.%d | build=0x%04X | state=%s | flags=0x%02X | fault=%d | PWM=%dHz CTRL=%dHz",u8(p,4),u8(p,5),u16(p,6),stateName(u8(p,8)),u8(p,9),u16(p,10),u16(p,12),u16(p,14));
                    return "D0 ohne DeltaESC-DESC-Signatur: "+Hex.of(p);
                case REG_ADC: if(p.length>=14)return String.format(Locale.US,"ADC off=%d/%d/%d raw=%d/%d/%d vbusRaw=%d",u16(p,0),u16(p,2),u16(p,4),u16(p,6),u16(p,8),u16(p,10),u16(p,12)); break;
                case REG_PHASE: if(p.length>=5)return String.format(Locale.US,"Phase map=%c%c%c signs=0x%02X dir=%s",mapChar(u8(p,0)),mapChar(u8(p,1)),mapChar(u8(p,2)),u8(p,3),u8(p,4)==0?"FWD":"REV"); break;
                case REG_R: if(p.length>=4)return String.format(Locale.US,"R = %.3f mΩ",u32(p,0)/1000.0); break;
                case REG_L: if(p.length>=4)return String.format(Locale.US,"L = %.3f µH",u32(p,0)/1000.0); break;
                case REG_FLUX: if(p.length>=4)return String.format(Locale.US,"Flux = %.3f mWb",u32(p,0)/1000.0); break;
                case REG_OBSERVER: if(p.length>=10)return String.format(Locale.US,"Observer ctrl=%d obs=%d err=%.2f° Iq=%.2fA Id=%.2fA",u16(p,0),u16(p,2),i16(p,4)*360.0/65536.0,i16(p,6)/1000.0,i16(p,8)/1000.0); break;
                case REG_LOCK: if(p.length>=8)return String.format(Locale.US,"Lock state=%s valid=%s lock=%d lost=%d confidence=%.1f%%",stateName(u8(p,0)),u8(p,1)!=0?"YES":"NO",u16(p,2),u16(p,4),u16(p,6)/10.0); break;
                case REG_TIMING: if(p.length>=12)return String.format(Locale.US,"ISR cycles last=%d max=%d budget=%d",u32(p,0),u32(p,4),u32(p,8)); break;
                case REG_FAULT: if(p.length>=4)return String.format(Locale.US,"Fault code=%d flags=0x%04X",u16(p,0),u16(p,2)); break;
                case 0x1A: if(p.length>=2)return String.format(Locale.US,"Stock FW raw 0x%04X",u16(p,0)); break;
            }
        }catch(Exception ignored){}
        return String.format(Locale.US,"cmd=0x%02X payload=%s",f.command,Hex.of(p));
    }
    static boolean isDeltaEscIdentity(NinebotProtocol.Frame f){
        byte[] p=f==null?null:f.payload;
        return f!=null&&f.command==REG_STATUS&&p!=null&&p.length==16&&p[0]=='D'&&p[1]=='E'&&p[2]=='S'&&p[3]=='C'&&u16(p,6)==EXPECTED_BUILD;
    }
    static int u8(byte[] p,int o){return p[o]&0xFF;} static int u16(byte[] p,int o){return(p[o]&0xFF)|((p[o+1]&0xFF)<<8);} static int i16(byte[] p,int o){return(short)u16(p,o);}
    static long u32(byte[] p,int o){return((long)u16(p,o))|(((long)u16(p,o+2))<<16);} static byte[] le16(int v){return new byte[]{(byte)v,(byte)(v>>>8)};}
    static int clamp(int v,int lo,int hi){return Math.max(lo,Math.min(hi,v));} static char mapChar(int v){return v==0?'A':v==1?'B':v==2?'C':'?';}
    static String stateName(int s){switch(s){case 0:return"STOP";case 1:return"ALIGN";case 2:return"OPEN";case 3:return"HANDOVER";case 4:return"CLOSED";case 5:return"FAULT";default:return"?"+s;}}
}
