package de.deltaesc.tool;
public final class TestMotorIdBenchProtocol {
 private static void eq(byte[] b,int... n){if(b.length!=n.length)throw new AssertionError("length");for(int i=0;i<n.length;i++)if((b[i]&255)!=n[i])throw new AssertionError("byte "+i);}
 public static void main(String[] a){
  eq(MotorIdBenchProtocol.start(),0x5A,0xA5,4,0x3E,0x20,0x03,0xE7,0xDE,0xC0,0x49,0x4D);
  eq(MotorIdBenchProtocol.stop(),0x5A,0xA5,0,0x3E,0x20,0x03,0xE6);
  eq(MotorIdBenchProtocol.status(0xF8),0x5A,0xA5,1,0x3E,0x20,0x02,0xF8,16);
  System.out.println("PASS Android E7/STOP/F8 Ninebot framing; encrypted by frozen BLE transport");
 }
}
