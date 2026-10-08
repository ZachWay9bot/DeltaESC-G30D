#!/usr/bin/env python3
"""Overlay guarded motor-parameter UI onto frozen working DashBLE v0.3.0.

DO NOT MODIFY NinebotCrypto, BLE queues or G30 framing. F0-F2 only, single
explicit human-confirmed write with exact build 0x0800, ACK, D3-D5 readback.
"""
from pathlib import Path
import hashlib,shutil,sys
root=Path(sys.argv[1]).resolve()
src=root/'app/src/main/java/de/deltaesc/tool'
expected={
 'BleUartClient.java':'3bbf0a9e06dd277ef30cecae9baf579f8f843b0e5db505853cfe78c916b14485',
 'NinebotCrypto.java':'57d88389891ad2fe5dc42dc69748c2a81dc24edc3930081a74ceee45ae035705',
 'NinebotProtocol.java':'866fb803a439faf78ce50655cf54cae15a4467766826088e0bfa86b8468e84a3'
}
def verify():
    for name,sha in expected.items():
        got=hashlib.sha256((src/name).read_bytes()).hexdigest()
        if got!=sha:raise RuntimeError(f'frozen v0.3.0 BLE crypto/transport hash mismatch: {name} {got}')
def once(s,old,new):
    c=s.count(old)
    if c!=1:raise RuntimeError(f'expected exactly one MainActivity anchor, got {c}: {old[:90]}')
    return s.replace(old,new,1)
verify()
f=src/'MainActivity.java'
s=f.read_text()
s=once(s,'import android.content.Context;', 'import android.content.Context;\nimport android.app.AlertDialog;\nimport android.text.InputType;\nimport android.widget.EditText;')
s=once(s,'/** Read-only link probe for the first DeltaESC G30D hardware validation. */',
       '/** G30 phone link and guarded configuration; NO MOTOR OR FIRMWARE commands. */')
s=once(s,'    private long motorNonce;', '''    private long motorNonce;
    private TextView configStatus;
    private final EditText[] configInputs=new EditText[3];
    private boolean configBusy,configReadAll,configVerify;
    private int configPending=-1,configNext=MotorConfig.FIRST_READ;
    private int configWriteReg=-1,configExpected=-1;
    private long configNonce;''')
s=once(s,' • v0.3.0 • READ ONLY',' • v0.3.1 • MOTOR CONFIG LOCKED')
s=once(s,'        root.addView(section("BUS LOG"));', '''        root.addView(section("MOTORPARAMETER · NUR DELTAESC 0x0800"));
        configStatus=text("Gesperrt: ESC muss DESC + genau Build 0x0800 melden",13,true);
        root.addView(configStatus);
        root.addView(text("Werte sind KEINE Motormessung. Nur nach externer R/L/Flux-Kalibrierung eingeben. Keine Motorstart-, E- oder Flashbefehle.",13,false));
        String[] captions={"R [mOhm]","L [uH]","Flux [mWb]"};
        for(int i=0;i<3;i++){
            final int x=i;
            LinearLayout line=row();
            TextView label=text(captions[i],13,false);
            line.addView(label,new LinearLayout.LayoutParams(dp(85),dp(45)));
            EditText input=new EditText(this);
            input.setSingleLine(true);
            input.setTextColor(Color.WHITE);
            input.setHint("Wert");
            input.setHintTextColor(Color.GRAY);
            input.setInputType(InputType.TYPE_CLASS_NUMBER|InputType.TYPE_NUMBER_FLAG_DECIMAL);
            configInputs[i]=input;
            line.addView(input,new LinearLayout.LayoutParams(0,dp(52),1));
            Button set=button("Schreiben",v->proposeMotorParamWrite(x));
            line.addView(set,new LinearLayout.LayoutParams(dp(125),dp(52)));
            root.addView(line);
        }
        Button readParams=button("R / L / Flux vom ESC lesen",v->beginConfigRead());
        readParams.setLayoutParams(full());root.addView(readParams);
        root.addView(section("BUS LOG"));''')
s=once(s,'        TextView foot=text("Keine Motor-Ansteuerung, keine Schreibbefehle, keine Updates. ST-Link ist nur für Recovery. Livewerte erst auf DeltaESC mit DA–DF verfügbar.",12,true);',
'''        TextView foot=text("Nur F0/F1/F2 Einzelparameter mit Bestätigung und Rücklesen, ausschließlich DESC Build 0800 (Gate-OFF-Build). KEINE E-Motorbefehle, keine SHU-Updates. ST-Link nur Recovery.",12,true);''')
s=once(s,'motorTelemetry.reset();if(motorState!=null)',
'''motorTelemetry.reset();configBusy=false;configReadAll=false;configVerify=false;configPending=-1;configWriteReg=-1;configNonce++;if(configStatus!=null)configStatus.setText("Gesperrt / getrennt");if(motorState!=null)''')
s=once(s,'        if(stage!=4 || f.src!=NinebotProtocol.ESC || f.cmd!=NinebotProtocol.READ_ACK)return;',
'''        if(stage!=4 || f.src!=NinebotProtocol.ESC)return;
        if(f.cmd==MotorConfig.WRITE_ACK){handleMotorConfigAck(f);return;}
        if(f.cmd!=NinebotProtocol.READ_ACK)return;
        if(f.arg>=MotorConfig.FIRST_READ&&f.arg<=MotorConfig.LAST_READ&&configBusy){
            handleMotorConfigRead(f);return;
        }''')
s=once(s,'    private void runReadProbe(){', '''    private boolean configAllowed(){
        return MotorConfig.eligible(stage==4,deltaDetected,deltaBuild);
    }
    private void configError(String why){
        configBusy=false;configReadAll=false;configVerify=false;
        configPending=-1;configWriteReg=-1;configNonce++;
        if(configStatus!=null)configStatus.setText("Konfiguration abgebrochen: "+why);
        append("CONFIG ERROR "+why);
    }
    private void configTimeout(int reg,long generation){
        if(configBusy&&configPending==reg&&configNonce==generation)
            configError(String.format(Locale.US,"Keine Bestätigung für %02X",reg));
    }
    private void beginConfigRead(){
        if(!configAllowed()){toast("Config gesperrt: nur DeltaESC Build 0800");return;}
        if(configBusy||motorCycle||motorAutomatic){toast("Zuerst laufende Diagnose stoppen");return;}
        configBusy=true;configVerify=false;configReadAll=true;
        configNext=MotorConfig.FIRST_READ;
        requestConfigRead(configNext);
    }
    private void requestConfigRead(int register){
        if(!configBusy||!configAllowed()){configError("Build- oder Sessionkennung verloren");return;}
        if(register<MotorConfig.FIRST_READ||register>MotorConfig.LAST_READ){
            configError("Ungültiges Rücklese-Register");return;
        }
        configPending=register;
        if(!sendEncrypted(NinebotProtocol.readEsc(register,4),"CONFIG READ")){
            configError("BLE-Warteschlange belegt, nichts gesendet");return;
        }
        long gen=++configNonce;
        handler.postDelayed(()->configTimeout(register,gen),1800);
    }
    private void handleMotorConfigRead(NinebotProtocol.Frame f){
        if(!configBusy||f.arg!=configPending)return;
        final long readValue;
        try{readValue=MotorConfig.value(f.payload);}
        catch(IllegalArgumentException e){configError(e.getMessage());return;}
        configNonce++;
        configPending=-1;
        append("CONFIG "+MotorConfig.present(f.arg,readValue));
        if(configVerify){
            boolean ok=readValue==Integer.toUnsignedLong(configExpected);
            configBusy=false;configVerify=false;configWriteReg=-1;
            configStatus.setText(ok?"Gespeichert in ESC-RAM und korrekt zurückgelesen: "+
                MotorConfig.present(f.arg,readValue):
                "FEHLER: Rücklesewert weicht vom gesendeten Wert ab!");
            if(!ok)append("CONFIG VERIFY MISMATCH");
            return;
        }
        int slot=f.arg-MotorConfig.FIRST_READ;
        configInputs[slot].setText(String.format(Locale.US,"%.3f",readValue/1000.0));
        if(configReadAll && f.arg<MotorConfig.LAST_READ){
            configNext=f.arg+1;
            handler.postDelayed(()->requestConfigRead(configNext),150);
        }else{
            configBusy=false;configReadAll=false;
            configStatus.setText("R/L/Flux aus ESC-RAM gelesen. Werte nicht als Messung interpretieren!");
        }
    }
    private void proposeMotorParamWrite(int index){
        if(!configAllowed()){toast("Gesperrt: nur DESC / Build 0x0800");return;}
        if(configBusy||motorCycle||motorAutomatic){toast("Laufenden Lesevorgang zuerst stoppen");return;}
        final int reg=MotorConfig.FIRST_WRITE+index;
        final int raw;
        try{raw=MotorConfig.parse(reg,configInputs[index].getText().toString());}
        catch(IllegalArgumentException e){toast(e.getMessage());return;}
        new AlertDialog.Builder(this)
            .setTitle("Nur "+MotorConfig.NAMES[index]+" einstellen?")
            .setMessage(MotorConfig.present(MotorConfig.expectedRead(reg),raw)+
                "\\n\\nNur für gemessene Motorparameter. Es wird genau EIN F-Register geschrieben, mit ACK und Rücklesen geprüft. Kein Motorstart.")
            .setNegativeButton("Abbrechen",null)
            .setPositiveButton("Wert schreiben",(dlg,which)->sendConfigWrite(reg,raw))
            .show();
    }
    private void sendConfigWrite(int reg,int raw){
        if(!configAllowed()||configBusy||motorCycle||motorAutomatic){
            toast("Konfigurations-Sperre aktiv");return;
        }
        final byte[] frame;
        try{frame=MotorConfig.writeFrame(reg,raw);}
        catch(IllegalArgumentException e){toast(e.getMessage());return;}
        configBusy=true;configReadAll=false;configVerify=false;
        configWriteReg=reg;configExpected=raw;configPending=reg;
        if(!sendEncrypted(frame,String.format(Locale.US,"CONFIG WRITE %02X",reg))){
            configError("BLE konnte nicht senden; Wert unverändert");return;
        }
        configStatus.setText("Warte auf ACK für "+MotorConfig.NAMES[reg-MotorConfig.FIRST_WRITE]);
        long gen=++configNonce;
        handler.postDelayed(()->configTimeout(reg,gen),1800);
    }
    private void handleMotorConfigAck(NinebotProtocol.Frame f){
        if(!configBusy||configWriteReg<0||f.arg!=configPending||
           f.arg!=configWriteReg)return;
        configNonce++;configPending=-1;
        if(!MotorConfig.successAck(f.payload)){
            configError("ESC hat Schreibanforderung abgelehnt (Status "+
                       (f.payload.length>0?(f.payload[0]&255):-1)+")");return;
        }
        configVerify=true;configReadAll=false;
        final int next=MotorConfig.expectedRead(configWriteReg);
        configStatus.setText("Schreiben quittiert; kontrolliere ESC-RAM-Rücklesewert");
        handler.postDelayed(()->requestConfigRead(next),150);
    }
    private void runReadProbe(){''')
s=once(s,'        if(motorCycle)return;\n        sendReadWhenFree(0x1A','        if(motorCycle||configBusy)return;\n        sendReadWhenFree(0x1A')
s=once(s,'        if(motorCycle){toast("Motor-Diagnose läuft; Snapshot erst danach");return;}',
       '        if(motorCycle||configBusy){toast("Diagnose oder Konfiguration beschäftigt");return;}')
s=once(s,'        if(motorCycle){if(auto)motorAutomatic=true;return;}',
       '        if(configBusy){toast("Konfiguration läuft");return;}\n        if(motorCycle){if(auto)motorAutomatic=true;return;}')
s=once(s,'        statusText.setText("Crypto paired: "',
'''        if(configStatus!=null&&!configBusy){
            configStatus.setText(configAllowed()?
                "DESC 0800: R/L/Flux einzeln lesbar und nach Bestätigung schreibbar (RAM)":
                "Gesperrt: Signatur DESC / exakt Build 0800 erforderlich");
        }
        statusText.setText("Crypto paired: "''')
# Literal string in Java belongs to a longer JSON String.format statement:
s=once(s,'DashBLE Motor 0.3.0','DashBLE Config 0.3.1')
# Leave 'read_only:true' in the report ONLY when no writes are currently
# possible; append capability fields in the JSON result instead.
# The report must not falsely claim a write-enabled v0.8.0 session is read-only.
s=once(s,'        json=json.substring(0,json.length()-1)',
         '        json=json.replace("\\\"read_only\\\":true","\\\"read_only\\\":"+(!configAllowed()));\n        json=json.substring(0,json.length()-1)')
s=once(s,'","+motorTelemetry.jsonFields()+"}"',
       '","+motorTelemetry.jsonFields()+",\\\\"config_guarded\\\\":true,\\\\"config_enabled\\\\":"+configAllowed()+"}"')
f.write_text(s)
shutil.copyfile(Path(__file__).with_name('MotorConfig.java'),src/'MotorConfig.java')
grad=root/'app/build.gradle'
t=grad.read_text()
t=once(t,"applicationId 'de.deltaesc.motorprobe'","applicationId 'de.deltaesc.motorparams'")
t=once(t,'versionCode 30','versionCode 31')
t=once(t,"versionName '0.3.0'","versionName '0.3.1'")
grad.write_text(t)
manifest=root/'app/src/main/AndroidManifest.xml'
t=manifest.read_text()
t=once(t,'android:label="DashBLE Motor"','android:label="DashBLE Config"')
manifest.write_text(t)
verify()
print('v0.3.1: F0-F2 config UI, exact-build guard, ACK+verified readback; crypto/transport untouched')
