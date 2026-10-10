package de.deltaesc.tool;

import java.util.Arrays;
import java.util.Locale;

/** Read-only proof of v0.9.18 FA/FB. No motor commands or mA claims. */
final class AdcProofTelemetry {
    static final int BUILD=0x0918, FA=0xFA, FB=0xFB, LENGTH=18;
    static final int SAMPLE=0x01, ADC1_OFFSET=0x02, ADC2_OFFSET=0x04;
    static final int RAW12=0x08, FRESH=0x10, CCER_OFF=0x20;
    static final int TERMINAL_KILL=0x40, ZERO_COUNTS=0x80;
    private byte[] fa,fb;
    private String error="";
    static int u16(byte[] b,int i){return (b[i]&255)|((b[i+1]&255)<<8);}
    static byte[] read(int reg){
        if(reg!=FA && reg!=FB)throw new IllegalArgumentException("Not an ADC proof page");
        return NinebotProtocol.build(NinebotProtocol.PHONE,NinebotProtocol.ESC,
                                     NinebotProtocol.READ,reg,new byte[]{(byte)LENGTH});
    }
    void reset(){fa=null;fb=null;error="";}
    String error(){return error;}
    boolean accept(int reg,byte[] data){
        if(reg!=FA && reg!=FB)return false;
        if(data==null || data.length!=LENGTH){
            error=String.format(Locale.US,"%02X: 18 Byte erwartet",reg);return false;
        }
        if((data[0]&255)!=1){
            error=String.format(Locale.US,"%02X: unbekannte Version %d",reg,data[0]&255);return false;
        }
        byte[] p=Arrays.copyOf(data,LENGTH);
        if(reg==FA)fa=p;else fb=p;
        return true;
    }
    boolean complete(){return fa!=null && fb!=null;}
    String report(){
        if(!error.isEmpty())return "ADC PROOF: "+error;
        if(fa==null && fb==null)return "FA/FB noch nicht gelesen";
        StringBuilder b=new StringBuilder();
        b.append("G30D DeltaESC v0.9.18 · ADC-ROHDATEN\n");
        if(fa!=null){
            final int f=fa[1]&255;
            b.append(String.format(Locale.US,"FA Flags: 0x%02X\n",f));
            for(int i=0;i<3;i++)
                b.append(String.format(Locale.US,"ADC1[%d] Roh %d / Offset %d / Δ %d Counts\n",
                    i,u16(fa,2+2*i),u16(fa,8+2*i),
                    u16(fa,2+2*i)-u16(fa,8+2*i)));
            b.append(String.format(Locale.US,"ADC-Alter: %d ms; Fehler: 0x%04X\n",
                u16(fa,14),u16(fa,16)));
            b.append("ADC1 Offset: ").append((f&ADC1_OFFSET)!=0?"plausibel":"UNGUELTIG")
             .append("; Rohwert: ").append((f&RAW12)!=0?"12 Bit OK":"UNGUELTIG").append("\n");
            b.append("Aktualitaet: ").append((f&FRESH)!=0?"ja":"NEIN").append("\n");
        }
        if(fb!=null){
            final int f=fb[1]&255;
            b.append(String.format(Locale.US,
                "FB Flags: 0x%02X | ADC2 CH4 Offset: %d, CH5 Offset: %d\n",
                f,u16(fb,2),u16(fb,4)));
            b.append(String.format(Locale.US,
                "Strom-Rohcount: last=%d peak=%d, raw/offset Peak=%d\n",
                u16(fb,6),u16(fb,8),u16(fb,16)));
            b.append(String.format(Locale.US,"TIM1 CCER=0x%04X BDTR=0x%04X\n",
                u16(fb,10),u16(fb,12)));
            b.append(String.format(Locale.US,"VBUS-Rohcount=%d\n",u16(fb,14)));
            b.append("Gate-Freigaben aus (Register): ")
              .append((f&CCER_OFF)!=0?"ja":"NEIN").append("\n");
            b.append("MOE-Kill verriegelt: ").append((f&TERMINAL_KILL)!=0?"ja":"nein").append("\n");
        }
        if(complete()){
            int flags=(fa[1]&255)&(fb[1]&255);
            b.append("Beide Seiten empfangen; keine gemeinsame zeitliche Momentaufnahme.\n");
            b.append("Ruhe-ADC <=24 Counts: ")
             .append((flags&ZERO_COUNTS)!=0?"ja":"NEIN");
        }
        b.append("\nRohcounts sind KEINE gemessenen Ampere. Physischer Gate-Off NICHT bestaetigt.");
        return b.toString();
    }
}
