package de.deltaesc.tool;

public final class TestLiveAdcV034 {
    private static void check(boolean ok,String message){if(!ok)throw new AssertionError(message);}
    private static void le16(byte[] p,int offset,int n){p[offset]=(byte)n;p[offset+1]=(byte)(n>>8);}
    public static void main(String[] args){
        LiveAdc adc=new LiveAdc();
        check(!LiveAdc.supported(false,true,0x0806),"crypto guard");
        check(!LiveAdc.supported(true,false,0x0806),"DESC guard");
        check(!LiveAdc.supported(true,true,0x0907),"stock lock");
        check(!LiveAdc.supported(true,true,0x0804),"motor write build must NOT read E8");
        check(!LiveAdc.supported(true,true,0x0807),"unknown future locked");
        check(LiveAdc.supported(true,true,0x0805),"old E8 readback");
        check(LiveAdc.supported(true,true,0x0806),"v086 E8 readback");
        check(!adc.accept(new byte[15]),"short E8");
        for(int sec=1;sec<=6;sec++){
            byte[] p=new byte[16];le16(p,0,2010);le16(p,2,2020);le16(p,4,1850);le16(p,6,10);
            p[8]=(byte)sec;
            p[9]=(byte)((sec==1||sec==6)?4:3);
            p[10]=(byte)((sec==4||sec==5)?4:5);
            p[11]=(byte)(2|8|16|32);
            le16(p,12,1);
            check(adc.accept(p),"E8 length "+sec);
            String report=adc.report();
            check(report.contains("Kanalpaar: plausibel"),"six-sector channel pair "+sec);
            check(report.contains("ADC-Werte/Kanalkennung"),"new integrity fault bit");
            check(report.contains("verzoegerter ADC-Trigger"),"new long-gap bit");
            check(report.contains("Abtastung schneller"),"new short-gap bit");
            check(report.contains("keine Motorfreigabe"),"no power");
            check(adc.jsonFields().contains("\"e8_complete\":true"),"JSON export");
        }
        adc.reset();
        check(!adc.hasSample(),"reset clears E8");
        check(adc.jsonFields().contains("\"e8_complete\":false"),"reset JSON");
        System.out.println("PASS E8 v0.8.5/v0.8.6 version gates, six sector reads, three new safety flags, stock lock");
    }
}
