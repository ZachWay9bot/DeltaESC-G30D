package de.deltaesc.tool;

import java.util.Arrays;
import java.util.Locale;

/**
 * Passive G30D motor ADC/FOC diagnostic decoding.
 * Transport and pairing are deliberately delegated to the unchanged v0.2.3
 * NinebotCrypto / BleUartClient / NinebotProtocol implementation.
 *
 * The ESC publishes six read-only 16-byte Ninebot registers DA..DF.
 * They are individual 256-sample windows, NOT a synchronized 96-byte record.
 */
final class MotorTelemetry {
    static final int FIRST=0xDA, LAST=0xDF;
    private final byte[][] page=new byte[6][];
    private String problem="";
    private int firstSequence=-1;
    private int windows=0;

    void reset(){
        Arrays.fill(page,null);
        firstSequence=-1;
        windows=0;
        problem="";
    }
    boolean accept(int reg,byte[] data) {
        if(reg<FIRST || reg>LAST) return false;
        if(data==null || data.length!=16){problem=String.format(Locale.US,"Register %02X: erwartete 16 Byte",reg);return false;}
        final int idx=reg-FIRST;
        if(reg==FIRST) {
            reset();
            if((data[0]&255)!=0x4D||(data[1]&255)!=0x54||(data[2]&255)!=0x52||(data[3]&255)!=0x30){
                problem="DA: MTR0-Kennung fehlt";return false;
            }
            long seq=u32(data,4);
            if((seq&1L)!=0L){problem="DA: ADC-Snapshot wird aktualisiert";return false;}
            firstSequence=(int)seq;
            windows=(int)u32(data,8);
        } else if(page[0]==null) {
            problem="DA muss zuerst gelesen werden";return false;
        }
        page[idx]=Arrays.copyOf(data,16);
        return true;
    }
    boolean complete(){
        for(byte[] p:page)if(p==null)return false;
        return ((u32(page[5],12)>>>16)&65535L)==1L;
    }
    String error(){return problem;}
    static int u16(byte[] d,int o){return(d[o]&255)|((d[o+1]&255)<<8);}
    static long u32(byte[] d,int o){return (u16(d,o)&65535L)|((long)u16(d,o+2)<<16);}
    static String hex(byte[] d) {
        StringBuilder b=new StringBuilder();
        for(byte x:d)b.append(String.format(Locale.US,"%02X",x&255));
        return b.toString();
    }
    String report(){
        if(page[0]==null)return problem.isEmpty()?"Noch keine Motordaten.":problem;
        StringBuilder b=new StringBuilder(512);
        b.append("MOTOR-DIAGNOSE · BLE 0xDA–0xDF\n");
        b.append("Zeit: ").append(u32(page[0],12)).append(" ms");
        b.append("   Fenster: ").append(windows);
        b.append("   Sequenz: ").append(firstSequence).append("\n");
        if(page[1]!=null && page[2]!=null && page[3]!=null){
            for(int i=0;i<4;i++){
                int mean=u16(page[1],2*i);
                int lo=u16(page[1],8+2*i);
                int hi=u16(page[2],2*i);
                int last=u16(page[2],8+2*i);
                b.append("ADC").append(i).append(": Ø ").append(mean).append("  ");
                b.append("Min ").append(lo).append("  Max ").append(hi);
                b.append("  p-p ").append(hi-lo).append("  Letzt ").append(last);
                if(i<3)b.append("  Offset ").append(u16(page[3],2*i));
                b.append("\n");
            }
        }
        if(page[3]!=null){
            long armed=u32(page[3],8);
            b.append("Gate-Status: ").append(armed==0?"AUS":"WARNUNG: "+armed).append("\n");
            b.append("Abtastungen: ").append(u32(page[3],12));
            b.append("  Fensterlänge: ").append(u16(page[3],6)).append("\n");
        }
        if(page[4]!=null){
            b.append("ADC-Intervall: ").append(u32(page[4],0)).append(" .. ")
             .append(u32(page[4],4)).append(" Zyklen\n");
            b.append(String.format(Locale.GERMANY,"FOC max: %d Zyklen (%.1f µs @72 MHz)\n",
                       u32(page[4],8),u32(page[4],8)/72.0));
            b.append(String.format(Locale.US,"TIM1 CCER 0x%08X\n",u32(page[4],12)));
        }
        if(page[5]!=null){
            b.append(String.format(Locale.US,"ADC JSQR 0x%08X  CR2 0x%08X  TIM1 BDTR 0x%08X\n",
                     u32(page[5],0),u32(page[5],4),u32(page[5],8)));
            b.append("Telemetrieprotokoll: ").append(u32(page[5],12)>>>16);
            b.append("  Fenster gültig: ").append((u32(page[5],12)&1)!=0?"JA":"NEIN").append("\n");
        }
        b.append("\nADC0–2: vermutete Phasenströme; ADC3: vermutete Spannung.");
        b.append("\nSkalierung/Vorzeichen unbestätigt. DA–DF sind zeitlich getrennte Lesevorgänge.");
        return b.toString();
    }
    String jsonFields(){
        StringBuilder b=new StringBuilder();
        b.append("\"motor_protocol\":1,\"motor_complete\":").append(complete())
         .append(",\"motor_sequence\":").append(firstSequence).append(",\"motor_windows\":").append(windows)
         .append(",\"motor_pages\":{");
        for(int i=0;i<6;i++) {
            if(i>0)b.append(',');
            b.append('"').append(String.format(Locale.US,"%02X",FIRST+i)).append("\":");
            if(page[i]==null)b.append("null");else b.append('"').append(hex(page[i])).append('"');
        }
        return b.append('}').toString();
    }
}
