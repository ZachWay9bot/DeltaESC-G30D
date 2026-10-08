#!/usr/bin/env python3
import sys, hashlib
from pathlib import Path
r=Path(sys.argv[1]).resolve()
java=r/'app/src/main/java/de/deltaesc/tool'
base={
'BleUartClient.java':'3bbf0a9e06dd277ef30cecae9baf579f8f843b0e5db505853cfe78c916b14485',
'NinebotCrypto.java':'57d88389891ad2fe5dc42dc69748c2a81dc24edc3930081a74ceee45ae035705',
'NinebotProtocol.java':'866fb803a439faf78ce50655cf54cae15a4467766826088e0bfa86b8468e84a3'}
for n,h in base.items(): assert hashlib.sha256((java/n).read_bytes()).hexdigest()==h,(n,'baseline hash mismatch')

def one(s,old,new):
    c=s.count(old)
    if c!=1: raise RuntimeError(f'anchor {c}: {old[:120]!r}')
    return s.replace(old,new,1)

(java/'MotorCommands.java').write_text('''package de.deltaesc.tool;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
final class MotorCommands {
 private MotorCommands(){}
 static final int BUILD_REQUIRED=0x0720,MAGIC=0xC0DE;
 static final int E0_CAL_OFFSET=0xE0,E5_START=0xE5,E6_STOP=0xE6;
 static final int F0_R_UOHM=0xF0,F1_L_NH=0xF1,F2_FLUX_UWB=0xF2,F3_PHASE=0xF3,F4_CURRENT_MA=0xF4;
 static byte[] magic(int arg){return write(arg,le16(MAGIC));}
 static byte[] stop(){return write(E6_STOP,new byte[0]);}
 static byte[] start(int ma){if(ma<100||ma>500)throw new IllegalArgumentException();ByteBuffer b=ByteBuffer.allocate(4).order(ByteOrder.LITTLE_ENDIAN);b.putShort((short)MAGIC);b.putShort((short)ma);return write(E5_START,b.array());}
 static byte[] u32(int arg,long v){ByteBuffer b=ByteBuffer.allocate(4).order(ByteOrder.LITTLE_ENDIAN);b.putInt((int)v);return write(arg,b.array());}
 static byte[] i16(int arg,int v){return write(arg,le16(v));}
 static byte[] write(int arg,byte[] p){return NinebotProtocol.build(NinebotProtocol.PHONE,NinebotProtocol.ESC,NinebotProtocol.WRITE,arg,p);}
 static String ack(int s){switch(s){case 0:return "OK";case 1:return "BUSY";case 2:return "UNSAFE";case 3:return "BAD_MAGIC";case 4:return "UNSUPPORTED";case 5:return "BAD_RANGE";case 6:return "RAM_ONLY";default:return "STATUS_"+s;}}
 private static byte[] le16(int v){return new byte[]{(byte)v,(byte)(v>>>8)};}
}
''')

m=java/'MainActivity.java';s=m.read_text()
s=one(s,'import android.app.Activity;','import android.app.Activity;\nimport android.app.AlertDialog;')
s=one(s,'import android.widget.Button;','import android.widget.Button;\nimport android.widget.EditText;')
s=one(s,'import android.widget.LinearLayout;','import android.widget.LinearLayout;\nimport android.text.InputType;')
s=one(s,'    private TextView motorState,motorData;','    private TextView motorState,motorData,benchState,diagText;\n    private EditText benchCurrent,cfgR,cfgL,cfgFlux,cfgPhase;')
s=one(s,'root.addView(text("DashBLE · DeltaESC Motor",28,true));','root.addView(text("G30 · DeltaESC Motor Bench",28,true));')
s=one(s,'TextView sub=text("G30D • bewährter G30-Transport • v0.3.0 • READ ONLY",13,false);','TextView sub=text("G30D • eingefrorener G30-Transport • v0.3.1 • MOTOR BENCH",13,false);')
old_ui='''        root.addView(section("MOTOR-DIAGNOSE · NUR LESEN"));
        motorState=text("Nur DeltaESC ab Build 0702",13,true);root.addView(motorState);
        LinearLayout mr=row();
        mr.addView(button("Einmal lesen",v->startMotorCycle(false)));
        mr.addView(button("Auto alle 3 s",v->startMotorCycle(true)));
        root.addView(mr);
        Button stop=button("Motor-Diagnose stoppen",v->stopMotor());
        stop.setLayoutParams(full());root.addView(stop);
        motorData=mono("ADC0–2 (Strom): Rohwerte, ohne Kalibrierung.\\nADC3 (Spannung): Rohwerte, ohne Teilerkalibrierung.\\nErst nach DeltaESC D0 / Build >=0702 verfügbar.");
        root.addView(motorData);
        Button copy=button("Bericht kopieren",v->copyReport());copy.setLayoutParams(full());root.addView(copy);'''
new_ui='''        root.addView(section("DIAG D0–D9"));
        diagText=mono("D0–D9 noch nicht gelesen.");root.addView(diagText);
        Button diag=button("D0–D9 Snapshot",v->readDeltaSnapshot());diag.setLayoutParams(full());root.addView(diag);

        root.addView(section("MOTOR TEST BENCH · BUILD 0720"));
        benchState=mono("Gesperrt, bis DeltaESC D0 Build 0x0720 bestätigt ist.");root.addView(benchState);
        benchCurrent=numberField("Teststrom mA","100");root.addView(benchCurrent,full());
        LinearLayout br=row();br.addView(button("E0 Offset kalibrieren",v->confirmOffsetCal()));br.addView(button("E5 TEST START",v->confirmBenchStart()));root.addView(br);
        Button estop=button("E6 STOP / DISARM",v->sendBenchStop());estop.setTextColor(Color.WHITE);estop.setBackgroundColor(Color.rgb(170,25,25));estop.setLayoutParams(full());root.addView(estop);

        root.addView(section("CFG · RAM ONLY"));
        cfgR=numberField("R mΩ","90");cfgL=numberField("L µH","100");cfgFlux=numberField("Flux mWb","1.800");cfgPhase=numberField("Phase °","0");
        root.addView(cfgR,full());root.addView(cfgL,full());root.addView(cfgFlux,full());root.addView(cfgPhase,full());
        LinearLayout c1=row();c1.addView(button("F0 R",v->writeR()));c1.addView(button("F1 L",v->writeL()));root.addView(c1);
        LinearLayout c2=row();c2.addView(button("F2 Flux",v->writeFlux()));c2.addView(button("F3 Phase",v->writePhase()));root.addView(c2);
        Button c3=button("F4 Teststrom in RAM",v->writeBenchCurrent());c3.setLayoutParams(full());root.addView(c3);
        TextView ram=text("F5 Persistenz ist in v0.7.2 nicht implementiert. Keine falsche Speichern-Taste.",12,false);ram.setTextColor(Color.LTGRAY);root.addView(ram);

        Button copy=button("Bericht kopieren",v->copyReport());copy.setLayoutParams(full());root.addView(copy);'''
s=one(s,old_ui,new_ui)
s=one(s,'TextView foot=text("Keine Motor-Ansteuerung, keine Schreibbefehle, keine Updates. ST-Link ist nur für Recovery. Livewerte erst auf DeltaESC mit DA–DF verfügbar.",12,true);','TextView foot=text("MOTOR BENCH ONLY: Hinterrad frei, 10S, erster Start 100 mA. Keine SHU-/IAP-Funktion in dieser App.",12,true);')

anchor='    private boolean motorSupported(){return stage==4 && deltaDetected && deltaBuild>=0x0702;}\n'
add='''    private boolean motorSupported(){return stage==4 && deltaDetected && deltaBuild>=0x0702;}
    private boolean benchSupported(){return stage==4&&deltaDetected&&deltaBuild==MotorCommands.BUILD_REQUIRED;}
    private EditText numberField(String hint,String value){EditText e=new EditText(this);e.setHint(hint);e.setText(value);e.setTextColor(Color.WHITE);e.setHintTextColor(Color.GRAY);e.setInputType(InputType.TYPE_CLASS_NUMBER|InputType.TYPE_NUMBER_FLAG_DECIMAL|InputType.TYPE_NUMBER_FLAG_SIGNED);return e;}
    private int benchMa(){int v=Integer.parseInt(benchCurrent.getText().toString().trim());if(v<100||v>500)throw new IllegalArgumentException();return v;}
    private void sendBench(byte[] p,String tag){if(!benchSupported()){toast("Nur DeltaESC Build 0x0720");return;}if(!sendEncrypted(p,tag))toast("BLE beschäftigt, erneut versuchen");}
    private void confirmOffsetCal(){if(!benchSupported()){toast("Build 0x0720 nicht bestätigt");return;}new AlertDialog.Builder(this).setTitle("E0 Stromoffset").setMessage("Motor aus, Rad frei. Offsetkalibrierung starten?").setNegativeButton("Abbrechen",null).setPositiveButton("E0 senden",(d,w)->sendBench(MotorCommands.magic(MotorCommands.E0_CAL_OFFSET),"E0 CAL OFFSET")).show();}
    private void confirmBenchStart(){if(!benchSupported()){toast("Build 0x0720 nicht bestätigt");return;}final int ma;try{ma=benchMa();}catch(Exception e){toast("Teststrom 100..500 mA");return;}new AlertDialog.Builder(this).setTitle("Sensorless Bench starten").setMessage("Nur Hinterrad frei und 10S. Erster Versuch 100 mA.\\n\\nTeststrom: "+ma+" mA").setNegativeButton("Abbrechen",null).setPositiveButton("E5 START",(d,w)->sendBench(MotorCommands.start(ma),"E5 START "+ma+"mA")).show();}
    private void sendBenchStop(){if(!benchSupported()){toast("Build 0x0720 nicht bestätigt");return;}motorAutomatic=false;motorCycle=false;motorPending=-1;motorNonce++;if(sendEncrypted(MotorCommands.stop(),"E6 STOP")&&benchState!=null)benchState.setText("E6 STOP gesendet");}
    private double val(EditText e){return Double.parseDouble(e.getText().toString().trim().replace(',','.'));}
    private void writeR(){try{long v=Math.round(val(cfgR)*1000.0);sendBench(MotorCommands.u32(MotorCommands.F0_R_UOHM,v),"F0 R");}catch(Exception e){toast("R ungültig");}}
    private void writeL(){try{long v=Math.round(val(cfgL)*1000.0);sendBench(MotorCommands.u32(MotorCommands.F1_L_NH,v),"F1 L");}catch(Exception e){toast("L ungültig");}}
    private void writeFlux(){try{long v=Math.round(val(cfgFlux)*1000.0);sendBench(MotorCommands.u32(MotorCommands.F2_FLUX_UWB,v),"F2 Flux");}catch(Exception e){toast("Flux ungültig");}}
    private void writePhase(){try{int v=(int)Math.round(val(cfgPhase)*65536.0/360.0);if(v<-32768||v>32767){toast("Phase außerhalb ±180°");return;}sendBench(MotorCommands.i16(MotorCommands.F3_PHASE,v),"F3 Phase");}catch(Exception e){toast("Phase ungültig");}}
    private void writeBenchCurrent(){try{int ma=benchMa();sendBench(MotorCommands.i16(MotorCommands.F4_CURRENT_MA,ma),"F4 Current");}catch(Exception e){toast("Teststrom 100..500 mA");}}
'''
s=one(s,anchor,add)

s=one(s,'        if(stage!=4 || f.src!=NinebotProtocol.ESC || f.cmd!=NinebotProtocol.READ_ACK)return;','''        if(stage!=4||f.src!=NinebotProtocol.ESC)return;
        if(f.cmd==NinebotProtocol.WRITE){int st=f.payload.length>0?(f.payload[0]&255):-1;String a=String.format(Locale.US,"ACK %02X = %s",f.arg,st<0?"leer":MotorCommands.ack(st));append(a);if(benchState!=null)benchState.setText(a);return;}
        if(f.cmd!=NinebotProtocol.READ_ACK)return;
        if(f.arg>=0xD0&&f.arg<=0xD9&&diagText!=null){String old=diagText.getText().toString();if(old.length()>5000)old=old.substring(0,3500);diagText.setText(describeDiag(f)+"\\n"+old);}''')

s=one(s,'int[] r={0xD0,0xD1,0xD8,0xD9};int d=0;','int[] r={0xD0,0xD1,0xD2,0xD3,0xD4,0xD5,0xD6,0xD7,0xD8,0xD9};int d=0;')

refresh='''    private void refreshStatus(){'''
diag='''    private String describeDiag(NinebotProtocol.Frame f){byte[]p=f.payload;try{switch(f.arg){
        case 0xD0:if(p.length>=16)return String.format(Locale.US,"D0 build=0x%04X state=%d flags=0x%02X fault=0x%04X pwm=%d ctrl=%d",u16(p,6),p[8]&255,p[9]&255,u16(p,10),u16(p,12),u16(p,14));break;
        case 0xD1:if(p.length>=14)return String.format(Locale.US,"D1 off=%d/%d/%d raw=%d/%d/%d aux=%d",u16(p,0),u16(p,2),u16(p,4),u16(p,6),u16(p,8),u16(p,10),u16(p,12));break;
        case 0xD2:if(p.length>=5)return String.format(Locale.US,"D2 map=%d/%d/%d signs=0x%02X dir=%d",p[0]&255,p[1]&255,p[2]&255,p[3]&255,p[4]&255);break;
        case 0xD3:if(p.length>=4)return String.format(Locale.US,"D3 R=%.3f mΩ",u32(p,0)/1000.0);break;
        case 0xD4:if(p.length>=4)return String.format(Locale.US,"D4 L=%.3f µH",u32(p,0)/1000.0);break;
        case 0xD5:if(p.length>=4)return String.format(Locale.US,"D5 Flux=%.3f mWb",u32(p,0)/1000.0);break;
        case 0xD6:if(p.length>=14)return String.format(Locale.US,"D6 phase=%d obs=%d err=%d iq=%dmA id=%dmA flux2=%d",u16(p,0),u16(p,2),i16(p,4),i16(p,6),i16(p,8),u32(p,10));break;
        case 0xD7:if(p.length>=10)return String.format(Locale.US,"D7 state=%d valid=%d lock=%d lost=%d conf=%d speed=%.2f eHz",p[0]&255,p[1]&255,u16(p,2),u16(p,4),u16(p,6),i16(p,8)/256.0);break;
        case 0xD8:if(p.length>=12)return String.format(Locale.US,"D8 cycles=%d/%d budget=%d",u32(p,0),u32(p,4),u32(p,8));break;
        case 0xD9:if(p.length>=8)return String.format(Locale.US,"D9 fault=0x%04X diag=0x%04X hardOC=%d phaseSum=%d",u16(p,0),u16(p,2),u16(p,4),u16(p,6));break;
    }}catch(Exception ignored){}return String.format(Locale.US,"D%X raw=%s",f.arg-0xD0,Hex.of(p));}

'''
s=one(s,refresh,diag+refresh)
s=one(s,'        statusText.setText("Crypto paired: "+(stage==4?"JA":"NEIN")+"\\nMIC Fehler: "+rxMicBad+"\\nFrame Fehler: "+rxPlainBad+"\\nESC FW: "+fw+"\\nESC Serial: "+(escSerial.isEmpty()?"-":escSerial)+"\\nDeltaESC Signatur: "+delta+"\\nMotor-Kommandos: NICHT IMPLEMENTIERT");','''        if(benchState!=null)benchState.setText(benchSupported()?"Build 0x0720 bestätigt · E0/E5/E6 + F0–F4 freigegeben":"Gesperrt · benötigt exakt DeltaESC Build 0x0720");
        statusText.setText("Crypto paired: "+(stage==4?"JA":"NEIN")+"\\nMIC Fehler: "+rxMicBad+"\\nFrame Fehler: "+rxPlainBad+"\\nESC FW: "+fw+"\\nESC Serial: "+(escSerial.isEmpty()?"-":escSerial)+"\\nDeltaESC Signatur: "+delta+"\\nMotor-Bench: "+(benchSupported()?"FREIGEGEBEN":"GESPERRT"));''')
s=s.replace('DashBLE Motor 0.3.0','DashBLE Motor Bench 0.3.1')
s=s.replace('\\"read_only\\":true','\\"read_only\\":false,\\"motor_bench_build_required\\":\\"0x0720\\"')
s=one(s,'private static int u16(byte[]p,int o){return(p[o]&255)|((p[o+1]&255)<<8);} private static String ascii','private static int u16(byte[]p,int o){return(p[o]&255)|((p[o+1]&255)<<8);} private static int i16(byte[]p,int o){return(short)u16(p,o);} private static long u32(byte[]p,int o){return(u16(p,o)&0xffffL)|((long)u16(p,o+2)<<16);} private static String ascii')
m.write_text(s)

g=r/'app/build.gradle';s=g.read_text();s=one(s,"applicationId 'de.deltaesc.motorprobe'","applicationId 'de.deltaesc.motorbench'");s=one(s,'versionCode 30','versionCode 31');s=one(s,"versionName '0.3.0'","versionName '0.3.1'");g.write_text(s)
manifest=r/'app/src/main/AndroidManifest.xml';s=manifest.read_text().replace('android:label="DashBLE Motor"','android:label="G30 Motor Bench"');manifest.write_text(s)
for n,h in base.items(): assert hashlib.sha256((java/n).read_bytes()).hexdigest()==h,(n,'transport mutated')
main=m.read_text();assert 'E5 TEST START' in main and 'D0–D9 Snapshot' in main and 'read_only\\":false' in main
print('v0.3.1 overlay PASS; transport byte-identical')
