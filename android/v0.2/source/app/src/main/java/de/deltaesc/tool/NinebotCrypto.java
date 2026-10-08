package de.deltaesc.tool;

import java.nio.charset.StandardCharsets;
import java.security.GeneralSecurityException;
import java.security.MessageDigest;
import java.util.Arrays;
import javax.crypto.Cipher;
import javax.crypto.spec.SecretKeySpec;

final class NinebotCrypto {
    private static final byte[] FW_DATA=hex("97CFB802844143DE56002B3B34780A5D");
    private final byte[] bleData=new byte[16], appData=new byte[16], shaKey=new byte[16], name;
    private int counter;
    NinebotCrypto(String deviceName){
        String n=(deviceName==null||deviceName.trim().isEmpty())?"NBScooter2020":deviceName;
        name=n.getBytes(StandardCharsets.UTF_8); deriveKey(name,FW_DATA);
    }
    int counter(){return counter;}

    byte[] encrypt(byte[] plain){
        if(!NinebotProtocol.valid(plain)) throw new IllegalArgumentException("Invalid Ninebot plaintext");
        byte[] body=Arrays.copyOfRange(plain,3,plain.length);
        byte[] out=new byte[plain.length+6]; System.arraycopy(plain,0,out,0,3);
        if(counter==0){
            byte[] tag=tagFirst(body), enc=cryptFirst(body); System.arraycopy(enc,0,out,3,enc.length);
            int e=plain.length; out[e]=0;out[e+1]=0;out[e+2]=tag[0];out[e+3]=tag[1];out[e+4]=0;out[e+5]=0;
            counter=1;
        }else{
            counter++;
            byte[] tag=tagNext(plain,counter), enc=cryptNext(body,counter); System.arraycopy(enc,0,out,3,enc.length);
            int e=plain.length; System.arraycopy(tag,0,out,e,4);out[e+4]=(byte)(counter>>>8);out[e+5]=(byte)counter;
        }
        if(plain.length>=23&&(plain[4]&255)==NinebotProtocol.BLE&&(plain[5]&255)==0x5C&&(plain[6]&255)==0)
            System.arraycopy(plain,7,appData,0,16);
        return out;
    }

    byte[] decryptVerified(byte[] enc){
        if(enc==null||enc.length<13||(enc[0]&255)!=0x5A||(enc[1]&255)!=0xA5) throw new SecurityException("bad encrypted frame");
        int plainLen=enc.length-6;
        if(plainLen!=(enc[2]&255)+7) throw new SecurityException("encrypted length mismatch");
        int rxCounter=(counter&0xFFFF0000)|((enc[enc.length-2]&255)<<8)|(enc[enc.length-1]&255);
        byte[] cipherBody=Arrays.copyOfRange(enc,3,plainLen);
        byte[] body=rxCounter==0?cryptFirst(cipherBody):cryptNext(cipherBody,rxCounter);
        byte[] plain=new byte[plainLen]; System.arraycopy(enc,0,plain,0,3);System.arraycopy(body,0,plain,3,body.length);
        int e=plainLen;
        if(rxCounter==0){
            byte[] t=tagFirst(body);
            if(enc[e]!=0||enc[e+1]!=0||enc[e+2]!=t[0]||enc[e+3]!=t[1]||enc[e+4]!=0||enc[e+5]!=0) throw new SecurityException("MIC0 mismatch");
        }else{
            byte[] t=tagNext(plain,rxCounter);
            for(int i=0;i<4;i++) if(enc[e+i]!=t[i]) throw new SecurityException("MIC mismatch");
            if((enc[e+4]&255)!=((rxCounter>>>8)&255)||(enc[e+5]&255)!=(rxCounter&255)) throw new SecurityException("counter mismatch");
        }
        if(!NinebotProtocol.valid(plain)) throw new SecurityException("plaintext format mismatch");
        NinebotProtocol.Frame f=NinebotProtocol.decode(plain);
        if(f.address==NinebotProtocol.BLE&&f.dst==NinebotProtocol.PHONE&&f.type==0x5B&&f.payload.length>=30){
            System.arraycopy(f.payload,0,bleData,0,16); deriveKey(name,bleData); return plain;
        }
        if(f.address==NinebotProtocol.BLE&&f.dst==NinebotProtocol.PHONE&&f.type==0x5C&&f.command==1) deriveKey(appData,bleData);
        if(counter>rxCounter) counter=rxCounter; else counter++;
        return plain;
    }
    private byte[] cryptFirst(byte[] d){
        byte[] out=new byte[d.length], ks=aes(FW_DATA,shaKey);
        for(int off=0;off<d.length;off+=16) for(int i=0;i<Math.min(16,d.length-off);i++) out[off+i]=(byte)(d[off+i]^ks[i]);
        return out;
    }
    private byte[] cryptNext(byte[] d,int c){
        byte[] out=new byte[d.length], ctr=counterBlock(c); int off=0;
        while(off<d.length){ctr[15]++;byte[] ks=aes(ctr,shaKey);int n=Math.min(16,d.length-off);for(int i=0;i<n;i++)out[off+i]=(byte)(d[off+i]^ks[i]);off+=n;}
        return out;
    }
    private static byte[] tagFirst(byte[] d){long s=0;for(byte b:d)s+=b;long x=~s;return new byte[]{(byte)x,(byte)(x>>8)};}
    private byte[] tagNext(byte[] plain,int c){
        byte[] state=counterBlock(c);int remaining=plain.length-3;state[0]=0x59;state[15]=(byte)remaining;byte[] chain=aes(state,shaKey);
        byte[] block=new byte[16];System.arraycopy(plain,0,block,0,Math.min(3,plain.length));chain=aes(xor(block,chain),shaKey);
        int off=3;while(remaining>0){int n=Math.min(16,remaining);block=new byte[16];System.arraycopy(plain,off,block,0,n);chain=aes(xor(block,chain),shaKey);remaining-=n;off+=n;}
        state[0]=0x01;state[15]=0;byte[] tail=aes(state,shaKey);
        return new byte[]{(byte)(tail[0]^chain[0]),(byte)(tail[1]^chain[1]),(byte)(tail[2]^chain[2]),(byte)(tail[3]^chain[3])};
    }
    private byte[] counterBlock(int c){byte[] b=new byte[16];b[0]=1;b[1]=(byte)(c>>>24);b[2]=(byte)(c>>>16);b[3]=(byte)(c>>>8);b[4]=(byte)c;System.arraycopy(bleData,0,b,5,8);return b;}
    private void deriveKey(byte[] a,byte[] b){try{byte[] in=new byte[32];if(a!=null)System.arraycopy(a,0,in,0,Math.min(16,a.length));if(b!=null)System.arraycopy(b,0,in,16,Math.min(16,b.length));byte[] h=MessageDigest.getInstance("SHA-1").digest(in);System.arraycopy(h,0,shaKey,0,16);}catch(GeneralSecurityException e){throw new IllegalStateException(e);}}
    private static byte[] aes(byte[] block,byte[] key){try{Cipher c=Cipher.getInstance("AES/ECB/NoPadding");c.init(Cipher.ENCRYPT_MODE,new SecretKeySpec(key,"AES"));return c.doFinal(Arrays.copyOf(block,16));}catch(GeneralSecurityException e){throw new IllegalStateException(e);}}
    private static byte[] xor(byte[] a,byte[] b){byte[] o=new byte[16];for(int i=0;i<16;i++)o[i]=(byte)(a[i]^b[i]);return o;}
    private static byte[] hex(String s){byte[] o=new byte[s.length()/2];for(int i=0;i<o.length;i++)o[i]=(byte)Integer.parseInt(s.substring(i*2,i*2+2),16);return o;}
}
