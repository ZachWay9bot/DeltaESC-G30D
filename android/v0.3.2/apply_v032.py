#!/usr/bin/env python3
"""Upgrade v0.3.1 GUI only to match the v0.8.4 F0-F5 atomic RAM protocol.
The frozen BLE, NinebotCrypto and Ninebot frame classes MUST remain byte-exact.
"""
from pathlib import Path
import hashlib,shutil,sys
root=Path(sys.argv[1]).resolve()
src=root/'app/src/main/java/de/deltaesc/tool'
expect={
 'BleUartClient.java':'3bbf0a9e06dd277ef30cecae9baf579f8f843b0e5db505853cfe78c916b14485',
 'NinebotCrypto.java':'57d88389891ad2fe5dc42dc69748c2a81dc24edc3930081a74ceee45ae035705',
 'NinebotProtocol.java':'866fb803a439faf78ce50655cf54cae15a4467766826088e0bfa86b8468e84a3'
}
def verify():
    for name,sha in expect.items():
        if hashlib.sha256((src/name).read_bytes()).hexdigest()!=sha:
            raise RuntimeError('Working G30 protocol modified: '+name)
def sub(s,a,b):
    if s.count(a)!=1:raise RuntimeError('Unexpected v0.3.1 source anchor ('+str(s.count(a))+'): '+a[:140])
    return s.replace(a,b,1)
verify()
main=src/'MainActivity.java';s=main.read_text()
s=sub(s,'new EditText[3]','new EditText[5]')
s=sub(s,'    private long configNonce;',
'''    private long configNonce;
    // 0=idle, 1=staging, 2=precommit status, 3=commit ACK,
    // 4=postcommit E7 status, 5=D3-D5 verify, 6=abort ACK, 7=abort E7 status.
    private int txnState,txnIndex;
    private int[] txnValues;''')
s=sub(s,'v0.3.1 • MOTOR CONFIG LOCKED','v0.3.2 • ATOMIC CONFIG LOCKED')
s=sub(s,'root.addView(section("MOTORPARAMETER · NUR DELTAESC 0x0800"));',
       'root.addView(section("MOTORPARAMETER · NUR DELTAESC 0x0804"));')
s=sub(s,'Gesperrt: ESC muss DESC + genau Build 0x0800 melden',
       'Gesperrt: ESC muss DESC + genau Build 0x0804 melden')
s=sub(s,'String[] captions={"R [mOhm]","L [uH]","Flux [mWb]"};',
       'String[] captions={"R [mOhm]","L [uH]","Flux [mWb]","Phase [Q16]","Teststrom [mA]"};')
s=sub(s,'for(int i=0;i<3;i++){\n            final int x=i;',
       'for(int i=0;i<5;i++){\n            final int x=i;')
s=sub(s,'input.setInputType(InputType.TYPE_CLASS_NUMBER|InputType.TYPE_NUMBER_FLAG_DECIMAL);',
       '''input.setInputType(InputType.TYPE_CLASS_NUMBER|
                (i<3?InputType.TYPE_NUMBER_FLAG_DECIMAL:
                 (i==3?InputType.TYPE_NUMBER_FLAG_SIGNED:0)));''')
s=sub(s,'            Button set=button("Schreiben",v->proposeMotorParamWrite(x));\n            line.addView(set,new LinearLayout.LayoutParams(dp(125),dp(52)));',
       '            // No individual writes: all five fields stage and commit together.')
s=sub(s,'        root.addView(section("BUS LOG"));',
'''        Button apply=button("Alle 5 Werte prüfen und übernehmen",v->proposeTxn());
        apply.setLayoutParams(full());root.addView(apply);
        Button cancelTxn=button("Angefangenen Parametersatz verwerfen",v->beginTxnAbort());
        cancelTxn.setLayoutParams(full());root.addView(cancelTxn);
        root.addView(section("BUS LOG"));''')
s=sub(s,'Nur F0/F1/F2 Einzelparameter mit Bestätigung und Rücklesen, ausschließlich DESC Build 0800 (Gate-OFF-Build). KEINE E-Motorbefehle, keine SHU-Updates. ST-Link nur Recovery.',
'''F0-F4 zunächst vollständig vormerken, mit E7 prüfen, dann atomar F5 übernehmen. Nur DESC Build 0804 (Gate-OFF). Kein E5, kein SHU. ST-Link nur Recovery.''')
s=sub(s,'configNonce++;if(configStatus!=null)',
       'configNonce++;txnState=0;txnIndex=0;txnValues=null;if(configStatus!=null)')
s=sub(s,'        if(f.arg>=MotorConfig.FIRST_READ&&f.arg<=MotorConfig.LAST_READ&&configBusy){',
'''        if(f.arg==MotorTxn.STATUS && configBusy){handleTxnStatus(f);return;}
        if(f.arg>=MotorConfig.FIRST_READ&&f.arg<=MotorConfig.LAST_READ&&configBusy){''')
start=s.index('    private boolean configAllowed(){')
end=s.index('    private void runReadProbe(){',start)
new=r'''    private boolean configAllowed(){
        return MotorTxn.permitted(stage==4,deltaDetected,deltaBuild);
    }
    private void configError(String why){
        configBusy=false;configReadAll=false;configVerify=false;
        configPending=-1;configWriteReg=-1;configNonce++;
        txnState=0;txnIndex=0;txnValues=null;
        configStatus.setText("Konfiguration angehalten: "+why+
            " (angefangenen ESC-Parametersatz gegebenenfalls verwerfen)");
        append("CONFIG STOP "+why);
    }
    private void configTimeout(int reg,long gen){
        if(configBusy&&configPending==reg&&configNonce==gen)
            configError(String.format(Locale.US,"Timeout bei %02X",reg));
    }
    private void beginConfigRead(){
        if(!configAllowed()){toast("Nur DeltaESC Build 0804");return;}
        if(configBusy||motorCycle||motorAutomatic){toast("Zuerst Diagnose stoppen");return;}
        configBusy=true;configReadAll=true;configVerify=false;txnState=0;
        configNext=MotorConfig.FIRST_READ;requestConfigRead(configNext);
    }
    private void requestConfigRead(int reg){
        if(!configBusy||!configAllowed()){configError("Session nicht bestätigt");return;}
        if(reg<MotorConfig.FIRST_READ||reg>MotorConfig.LAST_READ){configError("Read-Adresse gesperrt");return;}
        configPending=reg;
        if(!sendEncrypted(NinebotProtocol.readEsc(reg,4),"CONFIG READ")){
            configError("BLE TX nicht angenommen");return;
        }
        final long gen=++configNonce;
        handler.postDelayed(()->configTimeout(reg,gen),1800);
    }
    private void handleMotorConfigRead(NinebotProtocol.Frame f){
        if(!configBusy||f.arg!=configPending)return;
        final long got;
        try{got=MotorConfig.value(f.payload);}
        catch(IllegalArgumentException e){configError("Ungültiges D3-D5-Format");return;}
        configNonce++;configPending=-1;
        if(txnState==5){
            final int idx=f.arg-MotorConfig.FIRST_READ;
            if(idx!=txnIndex || got!=Integer.toUnsignedLong(txnValues[idx])){
                configError("R/L/Flux-Rücklesewert stimmt nicht überein");return;
            }
            if(++txnIndex<3){
                final int next=MotorConfig.FIRST_READ+txnIndex;
                handler.postDelayed(()->requestConfigRead(next),140);
            }else{
                configBusy=false;txnState=0;configStatus.setText(
                    "Alle 5 Werte atomar übernommen. E7 + D3/D4/D5 geprüft. Nur ESC-RAM.");
                append("PASS CONFIG COMMIT E7 + D3..D5");
            }
            return;
        }
        if(txnState!=0){configError("Unerwartete Config-Antwort");return;}
        int idx=f.arg-MotorConfig.FIRST_READ;
        configInputs[idx].setText(String.format(Locale.US,"%.3f",got/1000.0));
        if(configReadAll && idx<2){
            final int next=f.arg+1;
            handler.postDelayed(()->requestConfigRead(next),140);
        }else{
            configBusy=false;configReadAll=false;
            configStatus.setText("R/L/Flux aus ESC-RAM gelesen, nicht gemessen");
        }
    }
    private void proposeTxn(){
        if(!configAllowed()){toast("Gesperrt: Nur DESC 0804");return;}
        if(configBusy||motorCycle||motorAutomatic){toast("Laufende Diagnose erst stoppen");return;}
        String[] input=new String[5];
        for(int i=0;i<5;i++)input[i]=configInputs[i].getText().toString();
        final int[] values;
        try{values=MotorTxn.parseAll(input);}
        catch(IllegalArgumentException ex){toast(ex.getMessage());return;}
        new AlertDialog.Builder(this).setTitle("Alle 5 Motorparameter vormerken?")
          .setMessage("R / L / Flux / elektrischer Phasenoffset / Teststrom werden zunächst im ESC-RAM vorgemerkt. Nur nach externer Messung verwenden. Erst bei vollständigem E7-Status erfolgt F5-Commit. Es wird kein Motor gestartet.")
          .setNegativeButton("Abbrechen",null)
          .setPositiveButton("Vormerken und übernehmen",(d,w)->startTxn(values))
          .show();
    }
    private void startTxn(int[] v){
        if(!configAllowed()||configBusy||motorCycle||motorAutomatic)return;
        configBusy=true;configReadAll=false;configVerify=false;
        txnState=1;txnIndex=0;txnValues=v.clone();
        sendTxnWrite(0xF0,MotorTxn.frame(0xF0,txnValues[0]));
    }
    private void sendTxnWrite(int reg,byte[] frame){
        if(!configBusy||!configAllowed()){configError("Session verloren");return;}
        configPending=reg;
        if(!sendEncrypted(frame,String.format(Locale.US,"TX F%X TXN",reg&15))){
            configError("BLE TX nicht bestätigt");return;
        }
        configStatus.setText(String.format(Locale.US,"Warte auf F%X-ACK …",reg&15));
        final long gen=++configNonce;
        handler.postDelayed(()->configTimeout(reg,gen),1800);
    }
    private void requestTxnStatus(){
        if(!configBusy||!configAllowed()){configError("Session verloren");return;}
        configPending=MotorTxn.STATUS;
        if(!sendEncrypted(NinebotProtocol.readEsc(MotorTxn.STATUS,16),"READ E7")){
            configError("BLE E7 TX nicht angenommen");return;
        }
        final long gen=++configNonce;
        handler.postDelayed(()->configTimeout(MotorTxn.STATUS,gen),1800);
    }
    private void handleTxnStatus(NinebotProtocol.Frame f){
        if(!configBusy||f.arg!=configPending)return;
        final MotorTxn.Status st;
        try{st=new MotorTxn.Status(f.payload);}
        catch(IllegalArgumentException e){configError("E7 hat keine 16 Byte");return;}
        configNonce++;configPending=-1;
        if(st.gates!=0){configError("Sicherheitswarnung: Gates nicht AUS");return;}
        if(txnState==2){
            if(!st.readyForCommit()){configError("E7-Maske nicht vollständig 0x1F");return;}
            txnState=3;
            handler.postDelayed(()->sendTxnWrite(0xF5,MotorTxn.commit()),140);
        }else if(txnState==4){
            if(!st.committed(txnValues)){configError("E7 Commit-Status stimmt nicht überein");return;}
            txnState=5;txnIndex=0;
            handler.postDelayed(()->requestConfigRead(0xD3),140);
        }else if(txnState==7){
            if(st.pendingMask!=0){configError("Abbruch nicht bestätigt: E7 Maske ungleich 0");return;}
            configBusy=false;txnState=0;configStatus.setText("ESC-Parametersatz verworfen (F5 Abort bestätigt)");
        }else configError("Unerwarteter E7-Status");
    }
    private void handleMotorConfigAck(NinebotProtocol.Frame f){
        if(!configBusy||f.arg!=configPending)return;
        final int expected=(txnState==6)?MotorTxn.ACK_ABORT:MotorTxn.ACK_STAGE;
        if(!MotorTxn.ack(f.payload,expected)){
            configError("ESC hat Schreiben abgelehnt, Status "+
                 (f.payload.length>0?(f.payload[0]&255):-1));return;
        }
        configNonce++;configPending=-1;
        if(txnState==1 && f.arg==0xF0+txnIndex){
            if(++txnIndex<5){
                final int n=txnIndex;
                handler.postDelayed(()->sendTxnWrite(0xF0+n,MotorTxn.frame(0xF0+n,txnValues[n])),140);
            }else{
                txnState=2;
                handler.postDelayed(this::requestTxnStatus,140);
            }
        }else if(txnState==3 && f.arg==0xF5){
            txnState=4;
            handler.postDelayed(this::requestTxnStatus,140);
        }else if(txnState==6 && f.arg==0xF5){
            txnState=7;
            handler.postDelayed(this::requestTxnStatus,140);
        }else configError("Unerwarteter F-ACK");
    }
    private void beginTxnAbort(){
        if(!configAllowed()){toast("Nur DESC 0804");return;}
        if(configBusy||motorCycle||motorAutomatic){toast("Zuerst laufenden Vorgang beenden");return;}
        new AlertDialog.Builder(this).setTitle("Vorgemerkte Werte verwerfen?")
            .setMessage("Sendet ausschließlich F5-Abbruch mit Magic, danach E7-Kontrolle. Aktive Werte bleiben unverändert.")
            .setNegativeButton("Nein",null)
            .setPositiveButton("Verwerfen",(d,w)->{
                if(!configAllowed()||configBusy)return;
                configBusy=true;configReadAll=false;txnState=6;
                sendTxnWrite(0xF5,MotorTxn.abort());
            }).show();
    }
'''
s=s[:start]+new+s[end:]
s=sub(s,'Gesperrt: Signatur DESC / exakt Build 0800 erforderlich',
       'Gesperrt: Signatur DESC / exakt Build 0804 erforderlich')
s=sub(s,'DESC 0800: R/L/Flux einzeln lesbar und nach Bestätigung schreibbar (RAM)',
       'DESC 0804: fünf Motorwerte nur gemeinsam vormerken + committen (RAM)')
s=sub(s,'DashBLE Config 0.3.1','DashBLE Config 0.3.2')
s=sub(s,'versionName \'0.3.1\'','versionName \'0.3.2\'') if False else s
s=sub(s,'        json=json.substring(0,json.length()-1)',
       '        json=json.substring(0,json.length()-1)')
main.write_text(s)
shutil.copyfile(Path(__file__).with_name("MotorTxn.java"),src/"MotorTxn.java")
grad=root/'app/build.gradle';t=grad.read_text()
t=sub(t,"applicationId 'de.deltaesc.motorparams'","applicationId 'de.deltaesc.motorparams2'")
t=sub(t,"versionCode 31","versionCode 32")
t=sub(t,"versionName '0.3.1'","versionName '0.3.2'")
grad.write_text(t)
manifest=root/'app/src/main/AndroidManifest.xml';t=manifest.read_text()
t=sub(t,'android:label="DashBLE Config"','android:label="DashBLE Txn"')
manifest.write_text(t)
verify()
assert "sendConfigWrite" not in main.read_text() and "proposeMotorParamWrite" not in main.read_text()
assert "MotorTxn.permitted" in main.read_text() and "handleTxnStatus" in main.read_text()
print("DashBLE v0.3.2: five-field atomic F0-F5 commit, guarded by build 0804; transport unchanged")
