package de.deltaesc.tool;

import java.util.Arrays;
import java.util.Locale;

/** EBiCS 0x0034 revision 6, read-only. Not DeltaESC FA/FB and not a motor gate. */
final class R6ReadOnlyTelemetry {
    static final int FW=0x1A, REV=0xF0, D0=0xD0, E0=0xE0, D8=0xD8;
    static final int[] REGS={FW,REV,D0,E0,D8};
    static final int[] LENGTHS={2,2,16,32,16};
    private final byte[][] pages=new byte[REGS.length][];
    private String error="";
    static int u16(byte[] p,int i){return (p[i]&255)|((p[i+1]&255)<<8);}
    static long u32(byte[] p,int i){return ((long)u16(p,i))|(((long)u16(p,i+2))<<16);}
    static boolean supported(int reg){for(int r:REGS)if(r==reg)return true;return false;}
    static int length(int reg){for(int i=0;i<REGS.length;i++)if(REGS[i]==reg)return LENGTHS[i];throw new IllegalArgumentException("read-only register");}
    static byte[] read(int reg){
        return NinebotProtocol.build(NinebotProtocol.PHONE,NinebotProtocol.ESC,
            NinebotProtocol.READ,reg,new byte[]{(byte)length(reg)});
    }
    void reset(){Arrays.fill(pages,null);error="";}
    String error(){return error;}
    boolean accept(int reg,byte[] payload){
        int at=-1;for(int i=0;i<REGS.length;i++)if(REGS[i]==reg)at=i;
        if(at<0||payload==null||payload.length!=LENGTHS[at]){
            error=String.format(Locale.US,"Register %02X: ungueltige Antwortlaenge",reg);return false;
        }
        if(reg==FW && u16(payload,0)!=0x0034){error="Nicht EBiCS FW 0x0034";return false;}
        if(reg==REV && u16(payload,0)!=6){error="Nicht EBiCS Revision 6";return false;}
        if(reg==D0 && (u16(payload,0)!=0x4730||u16(payload,2)!=3)){
            error="Falsche D0 Signatur oder ABI";return false;
        }
        if(reg==D0 && (u16(payload,14)&1)==0){
            error="D0: Ausgangssperre nicht bestaetigt. Kein weiterer Diagnoselauf.";return false;
        }
        pages[at]=payload.clone();error="";return true;
    }
    boolean identity(){return pages[0]!=null&&pages[1]!=null&&pages[2]!=null;}
    String report(){
        StringBuilder s=new StringBuilder("EBiCS r6 | Nur lesen, kein Motorstart\n");
        if(!error.isEmpty())s.append("FEHLER: ").append(error).append('\n');
        if(pages[0]!=null)s.append(String.format(Locale.US,"FW 0x%04X\n",u16(pages[0],0)));
        if(pages[1]!=null)s.append("Revision ").append(u16(pages[1],0)).append('\n');
        if(pages[2]!=null){
            byte[] p=pages[2];int faults=u16(p,4);
            s.append(String.format(Locale.US,
                "D0 Signatur 0x%04X ABI %d | Fault 0x%04X\n",u16(p,0),u16(p,2),faults));
            s.append("Sample ungueltig: ").append((faults&1)!=0).append("; Deadline: ").append((faults&16)!=0).append('\n');
            s.append(String.format(Locale.US,
                "ISR Zyklen max (low16): %d | Kalibrierung %d | Inhibit 0x%04X\n",
                u16(p,6),u16(p,12),u16(p,14)));
        }
        if(pages[3]!=null){
            byte[] p=pages[3];long coreCycles=u16(p,6)|((long)u16(p,8)<<16);
            s.append(String.format(Locale.US,
                "Benchmark Status %d | %d/%d | Core max %d Takte | Worstcase %d\n",
                u16(p,0),u16(p,2),u16(p,4),coreCycles,u16(p,10)));
            s.append(String.format(Locale.US,
                "Benchmark Fehler 0x%04X | Verified %d | Generation %d | Scope %d\n",
                u16(p,12),u16(p,20),u16(p,18),u16(p,28)));
            s.append("Kernbudget: 11000 Takte. Messung ist NICHT der komplette ADC-ISR-Pfad.\n");
        }
        if(pages[4]!=null){
            byte[] p=pages[4];
            s.append("ADC raw counts:");
            for(int i=0;i<8;i++)s.append(' ').append(u16(p,i*2));
            s.append("\nReihenfolge IA IB IC VBAT VA VB VC IBAT. Keine kalibrierten Ampere.\n");
        }
        s.append("Keine automatische Freigabe von Gates, Motor-ID oder Fahrbetrieb.");
        return s.toString();
    }
}
