package de.deltaesc.tool;
import java.util.Arrays;
public final class TestMotorCommands {
  private static void eq(byte[] got, int... want){
    if(got.length!=want.length) throw new AssertionError("len "+got.length+" != "+want.length);
    for(int i=0;i<want.length;i++) if((got[i]&255)!=want[i]) throw new AssertionError("byte "+i+": "+(got[i]&255)+" != "+want[i]);
  }
  public static void main(String[] args){
    eq(MotorCommands.magic(MotorCommands.E0_CAL_OFFSET),0x5A,0xA5,2,0x3E,0x20,0x03,0xE0,0xDE,0xC0);
    eq(MotorCommands.start(100),0x5A,0xA5,4,0x3E,0x20,0x03,0xE5,0xDE,0xC0,0x64,0x00);
    eq(MotorCommands.stop(),0x5A,0xA5,0,0x3E,0x20,0x03,0xE6);
    eq(MotorCommands.u32(MotorCommands.F0_R_UOHM,90000),0x5A,0xA5,4,0x3E,0x20,0x03,0xF0,0x90,0x5F,0x01,0x00);
    eq(MotorCommands.i16(MotorCommands.F4_CURRENT_MA,100),0x5A,0xA5,2,0x3E,0x20,0x03,0xF4,0x64,0x00);
    boolean low=false,high=false;try{MotorCommands.start(99);}catch(IllegalArgumentException e){low=true;}try{MotorCommands.start(501);}catch(IllegalArgumentException e){high=true;}
    if(!low||!high) throw new AssertionError("current gate");
    System.out.println("MotorCommands vectors PASS");
  }
}
