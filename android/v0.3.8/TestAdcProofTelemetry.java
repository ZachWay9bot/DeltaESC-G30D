package de.deltaesc.tool;
public final class TestAdcProofTelemetry {
    static void check(boolean x,String name){if(!x)throw new AssertionError(name);}
    static void eq(byte[] p,int... expected){
        check(p.length==expected.length,"frame length");
        for(int i=0;i<p.length;i++)
            check((p[i]&255)==expected[i],"frame byte "+i);
    }
    static void put16(byte[] b,int pos,int v){b[pos]=(byte)v;b[pos+1]=(byte)(v>>>8);}
    public static void main(String[] unused){
        eq(AdcProofTelemetry.read(0xFA),0x5A,0xA5,1,0x3E,0x20,1,0xFA,18);
        eq(AdcProofTelemetry.read(0xFB),0x5A,0xA5,1,0x3E,0x20,1,0xFB,18);
        boolean rejected=false;
        try{AdcProofTelemetry.read(0xF9);}catch(IllegalArgumentException e){rejected=true;}
        check(rejected,"unknown registers rejected");
        AdcProofTelemetry p=new AdcProofTelemetry();
        check(!p.accept(0xFA,new byte[2]),"length rejected");
        check(!p.accept(0xFA,new byte[18]),"version rejected");
        byte[] fa=new byte[18],fb=new byte[18];fa[0]=1;fb[0]=1;
        fa[1]=(byte)0xBF;fb[1]=(byte)0xFF;
        for(int i=0;i<3;i++){put16(fa,2+2*i,2048+i);put16(fa,8+2*i,2048);}
        put16(fa,14,9);put16(fa,16,0xA02C);
        put16(fb,2,2047);put16(fb,4,2049);
        put16(fb,6,4);put16(fb,8,12);
        put16(fb,10,0x1000);put16(fb,12,0x0C40);
        put16(fb,14,2400);put16(fb,16,2);
        check(p.accept(0xFA,fa),"FA accepted");
        check(!p.complete(),"only FA incomplete");
        check(p.accept(0xFB,fb),"FB accepted");
        check(p.complete(),"both complete");
        String report=p.report();
        check(report.contains("CCER=0x1000"),"CCER decoded");
        check(report.contains("BDTR=0x0C40"),"BDTR decoded");
        check(report.contains("0xA02C"),"fault decoded");
        check(report.contains("2049"),"raw decoded");
        check(report.contains("KEINE gemessenen Ampere"),"no mA claim");
        byte[] bad=fb.clone();bad[0]=2;
        check(!p.accept(0xFB,bad),"unknown revision rejected");
        check(p.error().contains("Version"),"error reported");
        p.reset();check(!p.complete() && p.error().isEmpty(),"reset clean");
        System.out.println("PASS Android read-only FA/FB v0.9.19: request bytes, strict lengths/version, flags, fault, no writes");
    }
}
