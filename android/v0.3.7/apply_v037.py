#!/usr/bin/env python3
"""Version 0.3.7: real ADC read-only GUI for DeltaESC 0x0918.

Base is exact, already built v0.3.6. Keep all 3 verified G30 crypto / UART
files byte-identical. Remove E7/UI instead of adapting its build guard.
"""
import hashlib,shutil,sys
from pathlib import Path
root=Path(sys.argv[1]).resolve()
java=root/'app/src/main/java/de/deltaesc/tool'
pin={
'BleUartClient.java':'3bbf0a9e06dd277ef30cecae9baf579f8f843b0e5db505853cfe78c916b14485',
'NinebotCrypto.java':'57d88389891ad2fe5dc42dc69748c2a81dc24edc3930081a74ceee45ae035705',
'NinebotProtocol.java':'866fb803a439faf78ce50655cf54cae15a4467766826088e0bfa86b8468e84a3'}
def verify():
    for n,h in pin.items():
        if hashlib.sha256((java/n).read_bytes()).hexdigest()!=h:
            raise RuntimeError('v0.3.7 MUST retain working G30 BLE/Crypto bytes: '+n)
def one(s,old,new):
    n=s.count(old)
    if n!=1:
        raise RuntimeError(f'v0.3.7 drift, unique anchor {old[:70]!r} count={n}')
    return s.replace(old,new,1)
verify()
main=java/'MainActivity.java'
s=main.read_text()
s=one(s,'setContentView(buildUi());addMotorIdBenchControls();',
        'setContentView(buildUi());addAdcProofControls();')
s=one(s,
      'if(f.arg>=0xF7&&f.arg<=0xF9){String value=MotorIdBenchProtocol.describe(f.arg,f.payload);',
      '''if(f.arg==0xFA||f.arg==0xFB){onAdcProofPage(f.arg,f.payload);return;}
        if(f.arg>=0xF7&&f.arg<=0xF9){String value=MotorIdBenchProtocol.describe(f.arg,f.payload);''')
# The old v0.3.5 Motor-ID control method is appended at the end of the
# activity. Remove its E7 button/alert/callback rather than hiding it.
anchor='    private android.widget.TextView motorIdResultText;'
if s.count(anchor)!=1:raise RuntimeError('E7 control block not found')
tail=s[s.index(anchor):]
if 'private void addMotorIdBenchControls(){' not in tail or not s.rstrip().endswith('}'):
    raise RuntimeError('unexpected motor bench method layout')
if 'sendEncrypted(MotorIdBenchProtocol.start()' not in tail:
    raise RuntimeError('missing motor-start method, cannot prove removal')
s=s[:s.index(anchor)]
addon='''
    private final AdcProofTelemetry adcProof=new AdcProofTelemetry();
    private android.widget.TextView adcProofStatus;
    private int adcProofPending=-1;
    private int adcProofNonce=0;

    private boolean adcProofReady(){
        return stage==4 && deltaDetected && deltaBuild==AdcProofTelemetry.BUILD;
    }

    private void addAdcProofControls(){
        android.view.View v=((android.view.ViewGroup)findViewById(android.R.id.content)).getChildAt(0);
        if(v instanceof android.widget.ScrollView)
            v=((android.widget.ScrollView)v).getChildAt(0);
        if(!(v instanceof android.widget.LinearLayout))
            throw new IllegalStateException("Motor Bench root layout absent");
        android.widget.LinearLayout root=(android.widget.LinearLayout)v;
        android.widget.TextView title=new android.widget.TextView(this);
        title.setText("DeltaESC 0x0918 · ADC DIAGNOSE (NUR LESEN)");
        title.setTextSize(18);title.setTextColor(android.graphics.Color.WHITE);
        root.addView(title);
        adcProofStatus=mono("FA/FB noch nicht gelesen. Kein Motorstart. Kein kalibrierter Motorstrom.");
        root.addView(adcProofStatus);
        android.widget.Button read=new android.widget.Button(this);
        read.setText("ADC FA / FB auslesen");
        read.setAllCaps(false);
        read.setOnClickListener(vw -> startAdcProof());
        root.addView(read);
        android.widget.Button copy=new android.widget.Button(this);
        copy.setText("ADC-Bericht kopieren");copy.setAllCaps(false);
        copy.setOnClickListener(vw -> {
            android.content.ClipboardManager cm=(android.content.ClipboardManager)
                getSystemService(android.content.Context.CLIPBOARD_SERVICE);
            if(cm!=null) cm.setPrimaryClip(android.content.ClipData.newPlainText(
                "DeltaESC ADC proof FA FB",adcProof.report()));
            toast("ADC-Rohdaten kopiert");
        });
        root.addView(copy);
    }
    private void startAdcProof(){
        if(!adcProofReady()){
            toast("Nur für angemeldete DeltaESC Firmware 0x0918; kein Zugriff auf Stock.");
            return;
        }
        if(!ble.canAcceptFrame()){
            toast("Bluetooth belegt; erneut versuchen");return;
        }
        ++adcProofNonce;
        adcProof.reset();adcProofPending=AdcProofTelemetry.FA;
        if(adcProofStatus!=null)adcProofStatus.setText("FA wird gelesen ...");
        requestAdcProofPage(AdcProofTelemetry.FA,adcProofNonce);
    }
    private void requestAdcProofPage(int reg,int nonce){
        if(nonce!=adcProofNonce || !adcProofReady() ||
           adcProofPending!=reg)return;
        if(!ble.canAcceptFrame()){
            handler.postDelayed(()->requestAdcProofPage(reg,nonce),150L);
            return;
        }
        if(!sendEncrypted(AdcProofTelemetry.read(reg),
                          String.format(java.util.Locale.US,"ADC READ %02X",reg))){
            if(adcProofStatus!=null)adcProofStatus.setText("ADC BLE TX gescheitert");
            adcProofPending=-1;
            return;
        }
        handler.postDelayed(()->{
            if(nonce==adcProofNonce && adcProofPending==reg){
                if(adcProofStatus!=null)
                    adcProofStatus.setText(String.format(java.util.Locale.US,
                         "ADC-Register %02X antwortet nicht",reg));
                adcProofPending=-1;
            }
        },2000L);
    }
    private void onAdcProofPage(int reg,byte[] payload){
        if(!adcProofReady()||reg!=adcProofPending)return;
        if(!adcProof.accept(reg,payload)){
            adcProofPending=-1;
            if(adcProofStatus!=null)adcProofStatus.setText(adcProof.report());
            return;
        }
        if(adcProofStatus!=null)adcProofStatus.setText(adcProof.report());
        if(reg==AdcProofTelemetry.FA){
            adcProofPending=AdcProofTelemetry.FB;
            final int nonce=adcProofNonce;
            handler.postDelayed(()->requestAdcProofPage(AdcProofTelemetry.FB,nonce),150L);
        }else{
            adcProofPending=-1;
            append("ADC PROOF FA/FB: read-only complete; zero-count is not motor-current calibration");
        }
    }
'''
s+=addon+'}\n'
if 'sendEncrypted(MotorIdBenchProtocol.start()' in s:
    raise RuntimeError('unsafe E7 call still reachable in activity')
if 'addMotorIdBenchControls()' in s:raise RuntimeError('old E7 UI retained')
if 'sendEncrypted(AdcProofTelemetry.read(reg)' not in s:
    raise RuntimeError('new read route missing')
main.write_text(s)
shutil.copyfile(Path(__file__).with_name('AdcProofTelemetry.java'),java/'AdcProofTelemetry.java')
grad=root/'app/build.gradle'
g=grad.read_text()
g=one(g,"applicationId 'de.deltaesc.motorbench0914'",
      "applicationId 'de.deltaesc.adcproof0918'")
g=one(g,"versionCode 36","versionCode 37")
g=one(g,"versionName '0.3.6'","versionName '0.3.7'")
grad.write_text(g)
verify()
print('PASS Android 0.3.7: crypto SHA256 unchanged; E7 UI/callback removed; FA/FB read-only')
