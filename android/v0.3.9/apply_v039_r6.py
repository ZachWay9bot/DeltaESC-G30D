#!/usr/bin/env python3
"""Build EBiCS r6 read-only inspector from v0.3.8; never alter crypto transport."""
from pathlib import Path
import hashlib,shutil,sys
root=Path(sys.argv[1]).resolve()
j=root/'app/src/main/java/de/deltaesc/tool'
pin={
    'BleUartClient.java':'3bbf0a9e06dd277ef30cecae9baf579f8f843b0e5db505853cfe78c916b14485',
    'NinebotCrypto.java':'57d88389891ad2fe5dc42dc69748c2a81dc24edc3930081a74ceee45ae035705',
    'NinebotProtocol.java':'866fb803a439faf78ce50655cf54cae15a4467766826088e0bfa86b8468e84a3'}
def verify():
    for n,v in pin.items():assert hashlib.sha256((j/n).read_bytes()).hexdigest()==v,n
def one(s,old,new):
    assert s.count(old)==1,(old,s.count(old))
    return s.replace(old,new,1)
verify()
main=j/'MainActivity.java';s=main.read_text()
s=one(s,'setContentView(buildUi());addAdcProofControls();',
        'setContentView(buildUi());addR6Controls();')
s=one(s,'if(f.cmd!=NinebotProtocol.READ_ACK)return;',
        'if(f.cmd!=NinebotProtocol.READ_ACK)return;\n'
        '        if(r6Pending==f.arg && R6ReadOnlyTelemetry.supported(f.arg)){onR6Page(f.arg,f.payload);return;}')
# Fail closed: also block legacy E5/E6 / WRITE callbacks still present in historical
# activity code. Handshake stage 0..3 remains untouched; stage 4 permits only
# Ninebot CMD01 READ bodies. This is protection in addition to hiding E7 UI.
import re
pattern=r'(private\s+boolean\s+sendEncrypted\s*\(\s*byte\[\]\s+(\w+)\s*,\s*String\s+\w+\s*\)\s*\{)'
match=re.search(pattern,s)
if match is None:raise RuntimeError("Read-only gate cannot locate encrypted transport wrapper")
frame=match.group(2)
guard='\n        if(stage==4 && ('+frame+'==null || '+frame+'.length<7 || ('+frame+'[5]&255)!=NinebotProtocol.READ))return false;'
s=s[:match.end()]+guard+s[match.end():]
assert s.rstrip().endswith('}')
s=s.rstrip()[:-1]
s+="""
    // Uses the already working legacy encrypted G30 transport. NO escape writes.
    private final R6ReadOnlyTelemetry r6=new R6ReadOnlyTelemetry();
    private android.widget.TextView r6Status;
    private int r6Pending=-1, r6Nonce=0;

    private void addR6Controls(){
        android.view.View v=((android.view.ViewGroup)findViewById(android.R.id.content)).getChildAt(0);
        if(v instanceof android.widget.ScrollView)
            v=((android.widget.ScrollView)v).getChildAt(0);
        if(!(v instanceof android.widget.LinearLayout))
            throw new IllegalStateException("No linear root");
        android.widget.LinearLayout root=(android.widget.LinearLayout)v;
        android.widget.TextView title=new android.widget.TextView(this);
        title.setText("EBiCS v0.3.4-r6 | ADC / Timing | NUR LESEN");
        title.setTextColor(android.graphics.Color.WHITE);title.setTextSize(18);
        root.addView(title);
        r6Status=mono("Erst NinebotCrypto anmelden, dann Diagnose lesen.");
        root.addView(r6Status);
        android.widget.Button read=new android.widget.Button(this);
        read.setText("r6 Register lesen (1A, F0, D0, E0, D8)");
        read.setAllCaps(false);read.setOnClickListener(vw->startR6Read());
        root.addView(read);
        android.widget.Button copy=new android.widget.Button(this);
        copy.setText("Diagnose kopieren");copy.setAllCaps(false);
        copy.setOnClickListener(vw->{
            android.content.ClipboardManager cm=(android.content.ClipboardManager)
                getSystemService(android.content.Context.CLIPBOARD_SERVICE);
            if(cm!=null)cm.setPrimaryClip(android.content.ClipData.newPlainText("EBiCS r6 read-only",r6.report()));
            toast("EBiCS r6 Bericht kopiert");
        });
        root.addView(copy);
    }
    private void startR6Read(){
        if(stage!=4){toast("NinebotCrypto nicht angemeldet");return;}
        if(!ble.canAcceptFrame()){toast("BLE beschaeftigt");return;}
        ++r6Nonce;r6.reset();r6Pending=R6ReadOnlyTelemetry.FW;
        if(r6Status!=null)r6Status.setText("Lese Versionswort...");
        requestR6Page(r6Pending,r6Nonce);
    }
    private void requestR6Page(int reg,int nonce){
        if(stage!=4||nonce!=r6Nonce||r6Pending!=reg)return;
        if(!ble.canAcceptFrame()){
            handler.postDelayed(()->requestR6Page(reg,nonce),150L);
            return;
        }
        if(!sendEncrypted(R6ReadOnlyTelemetry.read(reg),
            String.format(java.util.Locale.US,"R6 READ %02X",reg))){
            r6Pending=-1;
            if(r6Status!=null)r6Status.setText("BLE READ gescheitert");
            return;
        }
        handler.postDelayed(()->{
            if(nonce==r6Nonce&&r6Pending==reg){
                r6Pending=-1;
                if(r6Status!=null)
                    r6Status.setText(r6.report()+String.format(java.util.Locale.US,
                        "\\nTimeout bei Register 0x%02X",reg));
            }
        },2500L);
    }
    private void onR6Page(int reg,byte[] payload){
        if(r6Pending!=reg||stage!=4)return;
        if(!r6.accept(reg,payload)){
            r6Pending=-1;
            if(r6Status!=null)r6Status.setText(r6.report());
            return;
        }
        if(r6Status!=null)r6Status.setText(r6.report());
        int[] order=R6ReadOnlyTelemetry.REGS;
        for(int i=0;i<order.length;i++)if(order[i]==reg){
            if(i+1<order.length){
                r6Pending=order[i+1];int next=r6Pending,nonce=r6Nonce;
                handler.postDelayed(()->requestR6Page(next,nonce),150L);
            }else{r6Pending=-1;append("EBiCS r6 read-only snapshot komplett");}
            break;
        }
    }
}
"""
assert 'sendEncrypted(MotorIdBenchProtocol.start()' not in s
assert 'addMotorIdBenchControls()' not in s
assert 'sendEncrypted(R6ReadOnlyTelemetry.read(reg)' in s
main.write_text(s)
shutil.copyfile(Path(__file__).with_name('R6ReadOnlyTelemetry.java'),j/'R6ReadOnlyTelemetry.java')
grad=root/'app/build.gradle';g=grad.read_text()
g=one(g,"applicationId 'de.deltaesc.adcproof0919'","applicationId 'de.deltaesc.ebicsr6diag'")
g=one(g,'versionCode 38','versionCode 39')
g=one(g,"versionName '0.3.8'","versionName '0.3.9-r6'")
grad.write_text(g)
verify()
print('PASS v0.3.9-r6 READ ONLY, exact original NinebotCrypto hashes, no motor launch')
