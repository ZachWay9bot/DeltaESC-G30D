package de.deltaesc.tool;

/** Host-only tests: no Android classes, no Bluetooth hardware required. */
public final class TestMotorTelemetry {
    static byte[] page(){return new byte[16];}
    static void u16(byte[] x,int off,int n){x[off]=(byte)n;x[off+1]=(byte)(n>>8);}
    static void u32(byte[] x,int off,long n){u16(x,off,(int)n);u16(x,off+2,(int)(n>>16));}
    static void require(boolean b,String error){if(!b)throw new AssertionError(error);}
    public static void main(String[] args){
        MotorTelemetry m=new MotorTelemetry();
        require(!m.complete(),"empty complete");
        require(!m.accept(0xDB,page()),"out-of-order accepted");
        require(!m.accept(0xDA,new byte[8]),"bad length accepted");
        byte[] a=page();a[0]='M';a[1]='T';a[2]='R';a[3]='0';
        u32(a,4,3);require(!m.accept(0xDA,a),"odd seq");
        u32(a,4,2);u32(a,8,4);u32(a,12,500);
        require(m.accept(0xDA,a),"DA");
        byte[] b=page(),c=page(),d=page(),e=page(),f=page();
        for(int i=0;i<4;i++){
            u16(b,2*i,2001+i);u16(b,8+2*i,1995+i);
            u16(c,2*i,2007+i);u16(c,8+2*i,2002+i);
        }
        for(int i=0;i<3;i++)u16(d,2*i,2000+i);
        u16(d,6,256);u32(d,8,0);u32(d,12,1024);
        u32(e,0,17777);u32(e,4,18100);u32(e,8,2200);u32(e,12,0);
        u32(f,0,0x01234567);u32(f,4,0xabcdefffL);u32(f,8,0x8877);u32(f,12,0x00010001);
        require(m.accept(0xDB,b)&&m.accept(0xDC,c)&&m.accept(0xDD,d)&&m.accept(0xDE,e)&&m.accept(0xDF,f),"pages");
        require(m.complete(),"complete");
        String report=m.report();
        require(report.contains("ADC0: Ø 2001")&&report.contains("Gate-Status: AUS"),"rendered values");
        require(report.contains("22")||report.contains("30"),"timing text");
        String json=m.jsonFields();
        require(json.contains("\"DA\":\"4D545230")&&json.contains("\"motor_complete\":true"),"JSON fields");
        byte[] bad=page();System.arraycopy(a,0,bad,0,16);bad[0]='X';
        require(!m.accept(0xDA,bad),"bad DA magic");
        require(m.error().contains("MTR0"),"error text");
        require(!m.complete(),"bad DA reset");
        System.out.println("PASS motor telemetry DA-DF decode, validity, snapshot and JSON");
    }
}
