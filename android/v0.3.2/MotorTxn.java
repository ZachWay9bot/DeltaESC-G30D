package de.deltaesc.tool;
import java.nio.charset.StandardCharsets;
import java.util.Locale;

/** v0.8.4 F0-F5 staged RAM transaction, NEVER a motor drive command. */
final class MotorTxn {
    static final int BUILD=0x0804;
    static final int MAGIC=0xC0DE;
    static final int STATUS=0xE7;
    static final int ACK_STAGE=6;
    static final int ACK_ABORT=0;
    static boolean permitted(boolean paired,boolean desc,int build) {
        return paired&&desc&&build==BUILD;
    }
    static int[] parseAll(String[] text){
        if(text==null||text.length!=5)throw new IllegalArgumentException("Alle fünf Felder sind nötig");
        int[] out=new int[5];
        for(int i=0;i<3;i++)out[i]=MotorConfig.parse(0xF0+i,text[i]);
        try{
            if(text[3]==null||!text[3].trim().matches("-?[0-9]{1,5}"))
                throw new NumberFormatException();
            out[3]=Integer.parseInt(text[3].trim());
            if(out[3]<-32768||out[3]>32767)throw new NumberFormatException();
        }catch(NumberFormatException ex){throw new IllegalArgumentException("Phasenoffset: signed Q16 -32768 bis 32767");}
        try{
            if(text[4]==null||!text[4].trim().matches("[0-9]{1,4}"))
                throw new NumberFormatException();
            out[4]=Integer.parseInt(text[4].trim());
            if(out[4]<100||out[4]>2000)throw new NumberFormatException();
        }catch(NumberFormatException ex){throw new IllegalArgumentException("Teststrom: 100 bis 2000 mA (keine Motorfreigabe)");}
        return out;
    }
    private static int u16(byte[] p,int at){return (p[at]&255)|((p[at+1]&255)<<8);}
    private static long u32(byte[] p,int at){return (u16(p,at)&65535L)|((long)u16(p,at+2)<<16);}
    private static void le16(byte[] p,int at,int n){p[at]=(byte)n;p[at+1]=(byte)(n>>>8);}
    static byte[] frame(int reg,int value){
        if(reg<0xF0||reg>0xF4)throw new IllegalArgumentException("Nur F0 bis F4 erlaubt");
        if(reg<0xF3) {
            int i=reg-0xF0;
            if(value<MotorConfig.MIN[i]||value>MotorConfig.MAX[i])
                throw new IllegalArgumentException("Parametergrenze überschritten");
        }else if(reg==0xF3 && (value<-32768||value>32767))
            throw new IllegalArgumentException("Phasenoffset außerhalb int16");
        else if(reg==0xF4 && (value<100||value>2000))
            throw new IllegalArgumentException("Strom außerhalb Bereich");
        byte[] p=new byte[reg<=0xF2?6:4];
        le16(p,0,MAGIC);
        le16(p,2,value);
        if(p.length==6)le16(p,4,value>>>16);
        return NinebotProtocol.build(NinebotProtocol.PHONE,NinebotProtocol.ESC,
                NinebotProtocol.WRITE,reg,p);
    }
    static byte[] commit(){
        return NinebotProtocol.build(NinebotProtocol.PHONE,NinebotProtocol.ESC,
                NinebotProtocol.WRITE,0xF5,new byte[]{(byte)MAGIC,(byte)(MAGIC>>8)});
    }
    static byte[] abort(){
        return NinebotProtocol.build(NinebotProtocol.PHONE,NinebotProtocol.ESC,
                NinebotProtocol.WRITE,0xF5,new byte[]{(byte)MAGIC,(byte)(MAGIC>>8),0});
    }
    static boolean ack(byte[] p,int expected){return p!=null&&p.length==1&&(p[0]&255)==expected;}
    static final class Status {
        final int pendingMask,activeValid,modelValid,gates,currentMa,guard;
        final long r,l;
        Status(byte[] p){
            if(p==null||p.length!=16)throw new IllegalArgumentException("E7 benötigt 16 Byte");
            pendingMask=p[0]&255;activeValid=p[1]&255;modelValid=p[2]&255;gates=p[3]&255;
            currentMa=u16(p,4);guard=u16(p,6);
            r=u32(p,8);l=u32(p,12);
        }
        boolean readyForCommit(){return pendingMask==0x1f&&gates==0;}
        boolean committed(int[] intended){
            return pendingMask==0&&activeValid==1&&modelValid==1&&gates==0
                &&currentMa==intended[4]&&r==Integer.toUnsignedLong(intended[0])
                &&l==Integer.toUnsignedLong(intended[1]);
        }
    }
    private MotorTxn(){}
}
