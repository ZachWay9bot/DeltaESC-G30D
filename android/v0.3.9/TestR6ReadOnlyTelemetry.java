package de.deltaesc.tool;
public final class TestR6ReadOnlyTelemetry {
    static void ok(boolean v,String msg){if(!v)throw new AssertionError(msg);}
    static void put16(byte[] p,int i,int n){p[i]=(byte)n;p[i+1]=(byte)(n>>>8);}
    static void checkRead(int reg,int length){
        byte[] f=R6ReadOnlyTelemetry.read(reg);
        ok(f.length==10,"request length");
        ok((f[0]&255)==0x5A&&(f[1]&255)==0xA5,"header");
        ok((f[2]&255)==1&&(f[3]&255)==0x3E&&(f[4]&255)==0x20,"phone to esc");
        ok((f[5]&255)==1&&(f[6]&255)==reg&&(f[7]&255)==length,"READ only");
    }
    public static void main(String[] unused){
        for(int i=0;i<R6ReadOnlyTelemetry.REGS.length;i++)
            checkRead(R6ReadOnlyTelemetry.REGS[i],R6ReadOnlyTelemetry.LENGTHS[i]);
        R6ReadOnlyTelemetry p=new R6ReadOnlyTelemetry();
        byte[] fw={0x34,0},rev={6,0},d0=new byte[16],e0=new byte[32],d8=new byte[16];
        put16(d0,0,0x4730);put16(d0,2,3);put16(d0,4,0x11);
        put16(d0,6,0x06AA);put16(d0,12,1);put16(d0,14,1);
        put16(e0,0,2);put16(e0,2,496);put16(e0,4,496);
        put16(e0,6,16456);put16(e0,12,8);
        ok(!p.accept(0xD0,new byte[2]),"D0 length guard");
        ok(p.accept(0x1A,fw),"build");
        ok(p.accept(0xF0,rev),"revision");
        ok(p.accept(0xD0,d0),"signature ABI inhibit");
        ok(p.identity(),"identity");
        ok(p.accept(0xE0,e0),"bench page");
        ok(p.accept(0xD8,d8),"ADC page");
        String report=p.report();
        ok(report.contains("0x0011"),"fault decode");
        ok(report.contains("16456"),"bench cycle high");
        ok(report.contains("496/496"),"counts");
        ok(report.contains("Keine kalibrierten Ampere"),"units");
        ok(report.contains("kein Motorstart"),"read only");
        p.reset();ok(!p.identity(),"reset");
        byte[] wrong=d0.clone();put16(wrong,14,0);
        ok(!p.accept(0xD0,wrong),"inhibit guard");
        ok(!p.accept(0x1A,new byte[]{0x26,1}),"stock reject");
        ok(!p.accept(0xF0,new byte[]{5,0}),"r5 reject");
        System.out.println("PASS EBiCS r6 guard + five READ commands + D0/E0/D8 parser; no active commands");
    }
}
