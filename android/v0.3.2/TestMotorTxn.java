package de.deltaesc.tool;
import java.util.Arrays;
public final class TestMotorTxn {
    static void yes(boolean b,String x){if(!b)throw new AssertionError(x);}
    static void fail(Runnable r){try{r.run();throw new AssertionError("unexpected acceptance");}catch(IllegalArgumentException expected){}}
    static byte[] wire(int arg,byte[] data){
        NinebotProtocol.Frame f=NinebotProtocol.decode(data);
        yes(f.src==NinebotProtocol.PHONE&&f.dst==NinebotProtocol.ESC,"address");
        yes(f.cmd==NinebotProtocol.WRITE&&f.arg==arg,"write command");
        return f.payload;
    }
    static void le16(byte[] p,int at,int x){p[at]=(byte)x;p[at+1]=(byte)(x>>8);}
    static void le32(byte[] p,int at,int x){le16(p,at,x);le16(p,at+2,x>>16);}
    public static void main(String[] args){
        yes(!MotorTxn.permitted(true,true,0x0907),"stock lock");
        yes(!MotorTxn.permitted(true,true,0x0800),"old direct-write build lock");
        yes(!MotorTxn.permitted(true,true,0x0803),"unverified old build lock");
        yes(!MotorTxn.permitted(true,false,0x0804),"signature gate");
        yes(!MotorTxn.permitted(false,true,0x0804),"auth gate");
        yes(MotorTxn.permitted(true,true,0x0804),"exact 0804");
        int[] v=MotorTxn.parseAll(new String[]{"100,500","150.250","12.500","-1234","350"});
        yes(Arrays.equals(v,new int[]{100500,150250,12500,-1234,350}),"unit decoding");
        fail(()->MotorTxn.parseAll(new String[]{"1","2","3","-40000","99"}));
        fail(()->MotorTxn.parseAll(new String[]{"0","1","2","0","350"}));
        for(int i=0;i<5;i++){
            byte[] data=wire(0xF0+i,MotorTxn.frame(0xF0+i,v[i]));
            yes(data.length==(i<3?6:4),"payload");
            yes((data[0]&255)==0xDE&&(data[1]&255)==0xC0,"magic");
            int raw=(data[2]&255)|((data[3]&255)<<8);
            if(i<3)raw|=(data[4]&255)<<16|((data[5]&255)<<24);
            if(i==3)yes((short)raw==-1234,"signed phase");
            else yes(raw==v[i],"parameter encoding");
        }
        byte[] commit=wire(0xF5,MotorTxn.commit());
        byte[] abort=wire(0xF5,MotorTxn.abort());
        yes(commit.length==2&&abort.length==3&&abort[2]==0,"F5 payload");
        fail(()->MotorTxn.frame(0xE5,350));
        fail(()->MotorTxn.frame(0xF5,350));
        yes(MotorTxn.ack(new byte[]{6},6),"stage ack");
        yes(!MotorTxn.ack(new byte[]{0},6),"stage rejection");
        yes(!MotorTxn.ack(new byte[]{6,0},6),"malformed ack");
        byte[] p=new byte[16];
        p[0]=31;p[1]=0;p[2]=0;p[3]=0;
        MotorTxn.Status st=new MotorTxn.Status(p);
        yes(st.readyForCommit(),"stage ready");
        p[0]=0;p[1]=1;p[2]=1;
        le16(p,4,350);le16(p,6,0xe000);le32(p,8,100500);le32(p,12,150250);
        st=new MotorTxn.Status(p);
        yes(st.committed(v),"committed fields");
        p[3]=1;yes(!new MotorTxn.Status(p).committed(v),"gate detection");
        p[3]=0;p[0]=1;yes(!new MotorTxn.Status(p).committed(v),"incomplete");
        fail(()->new MotorTxn.Status(new byte[8]));
        System.out.println("PASS phone transaction strict build, 5 writes, magic, F5, E7, ACK and guards");
    }
}
