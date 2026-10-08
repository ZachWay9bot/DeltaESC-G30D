#!/usr/bin/env python3
"""Add motor read-only GUI to verified G30 v0.2.3 app. Crypto/transport untouched."""
import sys, shutil, hashlib
from pathlib import Path
r=Path(sys.argv[1]).resolve()
java=r/'app/src/main/java/de/deltaesc/tool'
baseline={
 'BleUartClient.java':'3bbf0a9e06dd277ef30cecae9baf579f8f843b0e5db505853cfe78c916b14485',
 'NinebotCrypto.java':'57d88389891ad2fe5dc42dc69748c2a81dc24edc3930081a74ceee45ae035705',
 'NinebotProtocol.java':'866fb803a439faf78ce50655cf54cae15a4467766826088e0bfa86b8468e84a3'
}
for name,expect in baseline.items():
 actual=hashlib.sha256((java/name).read_bytes()).hexdigest()
 if actual!=expect:raise RuntimeError(f'v0.2.3 proven transport changed: {name}: {actual}')
def single(x,old,new):
 if x.count(old)!=1:raise RuntimeError(f'expected one anchor, got {x.count(old)}: {old[:110]!r}')
 return x.replace(old,new,1)
m=java/'MainActivity.java'
s=m.read_text()
s=single(s,
 '''    private int deltaBuild=-1,deltaFlags=-1;''',
 '''    private int deltaBuild=-1,deltaFlags=-1;
    private final MotorTelemetry motorTelemetry=new MotorTelemetry();
    private TextView motorState,motorData;
    private boolean motorAutomatic,motorCycle;
    private int motorPending=-1,motorNext=MotorTelemetry.FIRST,motorWaits;
    private long motorNonce;''')
s=single(s,
 '''root.addView(button("Bericht kopieren",v->copyReport()));''',
 '''root.addView(section("MOTOR-DIAGNOSE · NUR LESEN"));
        motorState=text("Nur DeltaESC ab Build 0702",13,true);root.addView(motorState);
        LinearLayout mr=row();
        mr.addView(button("Einmal lesen",v->startMotorCycle(false)));
        mr.addView(button("Auto alle 3 s",v->startMotorCycle(true)));
        root.addView(mr);
        Button stop=button("Motor-Diagnose stoppen",v->stopMotor());
        stop.setLayoutParams(full());root.addView(stop);
        motorData=mono("ADC0–2 (Strom): Rohwerte, ohne Kalibrierung.\\nADC3 (Spannung): Rohwerte, ohne Teilerkalibrierung.\\nErst nach DeltaESC D0 / Build >=0702 verfügbar.");
        root.addView(motorData);
        Button copy=button("Bericht kopieren",v->copyReport());copy.setLayoutParams(full());root.addView(copy);''')
s=single(s,
 '''"DeltaESC Link Probe",28,true''',
 '''"DashBLE · DeltaESC Motor",28,true''')
s=single(s,
 '''"G30D • NinebotCrypto 5A A5 • READ ONLY • v0.2.3"''',
 '''"G30D • bewährter G30-Transport • v0.3.0 • READ ONLY"''')
s=single(s,
 '''Diese v0.2.3 sendet keine E-/F-Kommandos und kann den Motor nicht freigeben. Sie dient nur zum Beweis von BLE-Crypto, ESC-Leseroute und DeltaESC-Signatur.''',
 '''Keine Motor-Ansteuerung, keine Schreibbefehle, keine Updates. ST-Link ist nur für Recovery. Livewerte erst auf DeltaESC mit DA–DF verfügbar.''')
s=single(s,
 '''private void resetSession(){handler.removeCallbacksAndMessages(null);wireParser.reset();''',
 '''private void resetSession(){motorAutomatic=false;motorCycle=false;motorPending=-1;motorNonce++;motorTelemetry.reset();if(motorState!=null)motorState.setText("Nicht verbunden");handler.removeCallbacksAndMessages(null);wireParser.reset();''')
s=single(s,
 '''        if(stage!=4 || f.src!=NinebotProtocol.ESC || f.cmd!=NinebotProtocol.READ_ACK)return;''',
 '''        if(stage!=4 || f.src!=NinebotProtocol.ESC || f.cmd!=NinebotProtocol.READ_ACK)return;
        if(f.arg>=MotorTelemetry.FIRST && f.arg<=MotorTelemetry.LAST){
            onMotorPage(f.arg,f.payload);
            return;
        }''')
s=single(s,
 '''    private void runReadProbe(){''',
 '''    private boolean motorSupported(){return stage==4 && deltaDetected && deltaBuild>=0x0702;}
    private void stopMotor(){
        motorAutomatic=false;motorCycle=false;motorPending=-1;motorNonce++;
        if(motorState!=null)motorState.setText("Motor-Diagnose gestoppt");
    }
    private void startMotorCycle(boolean auto){
        if(!motorSupported()){toast("DeltaESC Build >=0702 nötig, sonst keine Motorregister");return;}
        if(motorCycle){if(auto)motorAutomatic=true;return;}
        motorAutomatic=auto;motorCycle=true;motorNext=MotorTelemetry.FIRST;
        motorPending=-1;motorWaits=0;motorNonce++;
        motorTelemetry.reset();
        motorState.setText(auto?"Motor-Diagnose aktiv (alle 3 s)":"Motor-Diagnose: Einmalmessung");
        sendMotorNext();
    }
    private void sendMotorNext(){
        if(!motorCycle || !motorSupported()){stopMotor();return;}
        if(motorPending>=0)return;
        if(motorNext>MotorTelemetry.LAST){
            motorCycle=false;motorState.setText(motorTelemetry.complete()?"Messung empfangen · ADC nur Rohwerte":"Messung unvollständig");
            motorData.setText(motorTelemetry.report());
            if(motorAutomatic){handler.postDelayed(()->{if(motorAutomatic)startMotorCycle(true);},3000);}
            return;
        }
        if(!ble.canAcceptFrame()){
            if(++motorWaits>20){motorFail("BLE-Sendewarteschlange blockiert");return;}
            handler.postDelayed(this::sendMotorNext,120);return;
        }
        final int register=motorNext;
        motorPending=register;motorWaits=0;
        if(!sendEncrypted(NinebotProtocol.readEsc(register,16),String.format(Locale.US,"MOTOR READ %02X",register))){
            motorPending=-1;
            if(++motorWaits>20){motorFail("BLE TX nicht akzeptiert");return;}
            handler.postDelayed(this::sendMotorNext,120);return;
        }
        final long id=++motorNonce;
        handler.postDelayed(()->{
            if(motorCycle && motorPending==register && motorNonce==id){
                motorFail(String.format(Locale.US,"Timeout bei Motorregister %02X",register));
            }
        },1750);
    }
    private void onMotorPage(int reg,byte[] payload){
        if(!motorCycle || reg!=motorPending)return;
        motorPending=-1;motorNonce++;
        if(!motorTelemetry.accept(reg,payload)){
            motorFail(motorTelemetry.error());return;
        }
        motorData.setText(motorTelemetry.report());
        motorNext=reg+1;
        handler.postDelayed(this::sendMotorNext,90);
    }
    private void motorFail(String reason){
        motorAutomatic=false;motorCycle=false;motorPending=-1;motorNonce++;
        if(motorState!=null)motorState.setText("Motor-Diagnose angehalten: "+reason);
        append("MOTOR READ ERROR: "+reason);
    }
    private void runReadProbe(){''')
s=single(s,
 '''        if(stage!=4){toast("Bluetooth-Anmeldung noch nicht abgeschlossen");return;}
        sendReadWhenFree(0x1A''',
 '''        if(stage!=4){toast("Bluetooth-Anmeldung noch nicht abgeschlossen");return;}
        if(motorCycle)return;
        sendReadWhenFree(0x1A''')
s=single(s,
 '''        if(!deltaDetected){toast("DeltaESC-Signatur fehlt; Snapshot gesperrt");return;}''',
 '''        if(!deltaDetected){toast("DeltaESC-Signatur fehlt; Snapshot gesperrt");return;}
        if(motorCycle){toast("Motor-Diagnose läuft; Snapshot erst danach");return;}''')
s=single(s,
 '''        statusText.setText("Crypto paired: "''',
 '''        if(motorState!=null&&!motorCycle&&!motorAutomatic)
            motorState.setText(motorSupported()?"DeltaESC Build >=0702: Motordaten lesbar":"Motorwerte nur mit DeltaESC Build >=0702");
        statusText.setText("Crypto paired: "''')
s=single(s,
 '''String json=String.format(Locale.US,"{\\"tool\\":\\"DeltaESC Link Probe 0.2.3\\"''',
 '''String json=String.format(Locale.US,"{\\"tool\\":\\"DashBLE Motor 0.3.0\\"''')
s=single(s,
 '''        ClipboardManager cm=(ClipboardManager)getSystemService(Context.CLIPBOARD_SERVICE);cm.setPrimaryClip(ClipData.newPlainText("DeltaESC report",json));toast("Bericht kopiert");''',
 '''        json=json.substring(0,json.length()-1)+","+motorTelemetry.jsonFields()+"}";
        ClipboardManager cm=(ClipboardManager)getSystemService(Context.CLIPBOARD_SERVICE);cm.setPrimaryClip(ClipData.newPlainText("DeltaESC motor report",json));toast("Bericht kopiert");''')
m.write_text(s)
shutil.copyfile(Path(__file__).with_name('MotorTelemetry.java'),java/'MotorTelemetry.java')
grad=r/'app/build.gradle'
s=grad.read_text()
s=single(s,"applicationId 'de.deltaesc.tool'","applicationId 'de.deltaesc.motorprobe'")
s=single(s,'versionCode 23','versionCode 30')
s=single(s,"versionName '0.2.3'","versionName '0.3.0'")
grad.write_text(s)
manifest=r/'app/src/main/AndroidManifest.xml'
s=manifest.read_text()
s=single(s,'android:label="DeltaESC Tool"','android:label="DashBLE Motor"')
manifest.write_text(s)
for name,expect in baseline.items():
 if hashlib.sha256((java/name).read_bytes()).hexdigest()!=expect:raise RuntimeError('transport mutation detected')
assert '0xE0' not in (java/'MainActivity.java').read_text()
assert '0xF0' not in (java/'MainActivity.java').read_text()
print('DashBLE v0.3.0 created; v0.2.3 BLE crypto / GATT / packet files binary-identical')
