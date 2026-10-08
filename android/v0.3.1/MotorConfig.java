package de.deltaesc.tool;

import java.math.BigDecimal;
import java.util.Locale;

/** The ONLY writable ESC registers in this guarded app are F0/F1/F2.
 * Handset must first authenticate and read a DESC signature at exact
 * v0.8.0 (0x0800), whose power stage is compile-disabled.
 */
final class MotorConfig {
    static final int REQUIRED_BUILD=0x0800;
    static final int FIRST_WRITE=0xF0, LAST_WRITE=0xF2;
    static final int FIRST_READ=0xD3, LAST_READ=0xD5;
    static final int WRITE_ACK=0x05;
    static final int[] MIN={1000,1000,100};
    static final int[] MAX={2000000,5000000,1000000};
    static final String[] NAMES={"R","L","Flux"};
    static final String[] UNITS={"mOhm","uH","mWb"};

    static boolean eligible(boolean authenticated,boolean deltaSignature,int build) {
        return authenticated && deltaSignature && build==REQUIRED_BUILD;
    }
    static int parse(int reg,String display) {
        if(reg<FIRST_WRITE||reg>LAST_WRITE)throw new IllegalArgumentException("Register gesperrt");
        if(display==null)throw new IllegalArgumentException("Wert fehlt");
        final String s=display.trim().replace(',','.');
        if(s.length()==0||s.length()>14||!s.matches("[0-9]+(\\.[0-9]{1,3})?"))
            throw new IllegalArgumentException("Nur positive Dezimalzahlen mit maximal 3 Nachkommastellen");
        final int index=reg-FIRST_WRITE;
        final BigDecimal scaled=new BigDecimal(s).multiply(BigDecimal.valueOf(1000));
        final int value;
        try{value=scaled.intValueExact();}
        catch(ArithmeticException e){throw new IllegalArgumentException("Wert ist nicht darstellbar");}
        if(value<MIN[index]||value>MAX[index])
            throw new IllegalArgumentException(NAMES[index]+": erlaubt "+
                String.format(Locale.US,"%.3f .. %.3f %s",MIN[index]/1000.0,
                       MAX[index]/1000.0,UNITS[index]));
        return value;
    }
    static byte[] writeFrame(int reg,int scaled){
        if(reg<FIRST_WRITE||reg>LAST_WRITE)throw new IllegalArgumentException("Write-Register nicht erlaubt");
        final int i=reg-FIRST_WRITE;
        if(scaled<MIN[i]||scaled>MAX[i])throw new IllegalArgumentException("Write-Wert nicht erlaubt");
        final byte[] raw={(byte)scaled,(byte)(scaled>>8),(byte)(scaled>>16),(byte)(scaled>>24)};
        return NinebotProtocol.build(NinebotProtocol.PHONE,NinebotProtocol.ESC,
                                     NinebotProtocol.WRITE,reg,raw);
    }
    static int expectedRead(int writeReg){return FIRST_READ+writeReg-FIRST_WRITE;}
    static long value(byte[] data){
        if(data==null||data.length!=4)throw new IllegalArgumentException("Readback nicht exakt 4 Byte");
        return (data[0]&255L)|((data[1]&255L)<<8)|((data[2]&255L)<<16)|((data[3]&255L)<<24);
    }
    static boolean successAck(byte[] data){return data!=null&&data.length==1&&data[0]==0;}
    static String present(int reg,long raw){
        if(reg<FIRST_READ||reg>LAST_READ)throw new IllegalArgumentException("Readback-Register gesperrt");
        int i=reg-FIRST_READ;
        return String.format(Locale.GERMANY,"%s: %.3f %s",NAMES[i],raw/1000.0,UNITS[i]);
    }
    private MotorConfig(){}
}
