package de.deltaesc.tool;

import java.util.Arrays;

/** Pure-Java tests: no hardware writes or Android runtime. */
public final class TestMotorConfig {
    private static void ok(boolean condition,String message){
        if(!condition)throw new AssertionError(message);
    }
    private static void rejects(Runnable x,String why){
        try{x.run();throw new AssertionError("should reject "+why);}
        catch(IllegalArgumentException expected){}
    }
    public static void main(String[] args){
        ok(!MotorConfig.eligible(true,false,0x0800),"signature");
        ok(!MotorConfig.eligible(false,true,0x0800),"auth");
        ok(!MotorConfig.eligible(true,true,0x0702),"legacy");
        ok(!MotorConfig.eligible(true,true,0x0801),"future active build");
        ok(!MotorConfig.eligible(true,true,0x0907),"stock raw version");
        ok(MotorConfig.eligible(true,true,0x0800),"exact gated build");
        int r=MotorConfig.parse(0xF0,"100,250");
        int l=MotorConfig.parse(0xF1,"150.125");
        int f=MotorConfig.parse(0xF2,"12.000");
        ok(r==100250 && l==150125 && f==12000,"units");
        rejects(()->MotorConfig.parse(0xE5,"100"),"drive register");
        rejects(()->MotorConfig.parse(0xF3,"100"),"phase mapping");
        rejects(()->MotorConfig.parse(0xF0,"0"),"zero resistance");
        rejects(()->MotorConfig.parse(0xF2,"0,099"),"flux lower bound");
        rejects(()->MotorConfig.parse(0xF1,"5000.001"),"inductance upper bound");
        rejects(()->MotorConfig.parse(0xF0,"-1"),"negative");
        rejects(()->MotorConfig.parse(0xF1,"1e4"),"exponent");
        rejects(()->MotorConfig.parse(0xF2,"1.2345"),"precision");
        rejects(()->MotorConfig.parse(0xF2,"NaN"),"NaN");
        rejects(()->MotorConfig.parse(0xF0,"2147483647"),"overflow");
        int[] registers={0xF0,0xF1,0xF2};
        int[] vals={r,l,f};
        for(int i=0;i<3;i++){
            byte[] pkt=MotorConfig.writeFrame(registers[i],vals[i]);
            ok(NinebotProtocol.valid(pkt),"frame");
            NinebotProtocol.Frame decoded=NinebotProtocol.decode(pkt);
            ok(decoded.src==0x3E&&decoded.dst==0x20,"addresses");
            ok(decoded.cmd==0x03&&decoded.arg==registers[i],"only config write");
            ok(decoded.payload.length==4,"payload width");
            long got=MotorConfig.value(decoded.payload);
            ok(got==vals[i],"little endian");
            ok(MotorConfig.expectedRead(registers[i])==0xD3+i,"readback address");
        }
        rejects(()->MotorConfig.writeFrame(0xE5,1000),"ESC arm");
        rejects(()->MotorConfig.writeFrame(0xF5,1000),"config flash commit");
        rejects(()->MotorConfig.writeFrame(0xF0,0),"invalid write value");
        rejects(()->MotorConfig.value(new byte[3]),"short readback");
        ok(MotorConfig.successAck(new byte[]{0}),"ACK OK");
        ok(!MotorConfig.successAck(new byte[]{1}),"ACK BUSY");
        ok(!MotorConfig.successAck(new byte[]{2}),"ACK UNSAFE");
        ok(!MotorConfig.successAck(new byte[]{0,0}),"malformed ACK");
        ok(MotorConfig.present(0xD5,12000).contains("12,000"),"format");
        System.out.println("PASS v0.3.1 Motorola R/L/Flux range, exact-build interlock, whitelist F0-F2, ACK & LE readback");
    }
}
