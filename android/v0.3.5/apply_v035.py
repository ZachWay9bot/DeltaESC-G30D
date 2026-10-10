#!/usr/bin/env python3
"""Motor-ID R/L/Flux bench app overlay on frozen G30 v0.3.3 NinebotCrypto.
Only MainActivity, MotorBenchProtocol and build metadata change.
Never touches BLE UART framing, pairing, crypto, authentication or MIC.
"""
from pathlib import Path
import hashlib,sys
r=Path(sys.argv[1]).resolve()
j=r/'app/src/main/java/de/deltaesc/tool'
frozen={
'BleUartClient.java':'3bbf0a9e06dd277ef30cecae9baf579f8f843b0e5db505853cfe78c916b14485',
'NinebotCrypto.java':'57d88389891ad2fe5dc42dc69748c2a81dc24edc3930081a74ceee45ae035705',
'NinebotProtocol.java':'866fb803a439faf78ce50655cf54cae15a4467766826088e0bfa86b8468e84a3'}
def check():
    for n,h in frozen.items():
        got=hashlib.sha256((j/n).read_bytes()).hexdigest()
        if got!=h:raise RuntimeError('Frozen BLE transport mismatch: '+n)
def change_once(text,old,new):
    if text.count(old)!=1: raise RuntimeError('Cannot locate unique source anchor '+repr(old[:100]))
    return text.replace(old,new,1)
check()
main=j/'MainActivity.java'
proto=j/'MotorBenchProtocol.java'
p=proto.read_text()
p=change_once(p,'BUILD_MOTOR_BENCH_V072 = 0x0720','BUILD_MOTOR_ID_0913 = 0x0913')
proto.write_text(p)
s=main.read_text().replace('BUILD_MOTOR_BENCH_V072','BUILD_MOTOR_ID_0913')
# Keep the proven authentication, framing and MIC unchanged. Insert the new
# controls only after the existing activity has built its original UI.
s=change_once(s,'setContentView(buildUi());','setContentView(buildUi());addMotorIdBenchControls();')
# Prevent any copy implying the 0x0720 E5 features are a 0x0913 control.
s=s.replace('v0.7.2','v0.9.13').replace('0x0720','0x0913')
# One session has to be visibly confirmed by the user. The app cannot know if
# the wheel is actually clear, so the firmware still checks all actual guards.
control='''
    private void addMotorIdBenchControls(){
        android.view.View v=((android.view.ViewGroup)findViewById(android.R.id.content)).getChildAt(0);
        if(v instanceof android.widget.ScrollView)
            v=((android.widget.ScrollView)v).getChildAt(0);
        if(!(v instanceof android.widget.LinearLayout))
            throw new IllegalStateException("Motor Bench root layout absent");
        android.widget.LinearLayout root=(android.widget.LinearLayout)v;
        android.widget.TextView heading=new android.widget.TextView(this);
        heading.setText("DELTAESC 0x0913: AUTO R / L / FLUX");
        heading.setTextSize(18);heading.setTextColor(android.graphics.Color.WHITE);
        root.addView(heading);
        android.widget.Button start=new android.widget.Button(this);
        start.setText("Motor Detect starten (E7)");
        start.setAllCaps(false);
        start.setOnClickListener(clicked -> {
            if(stage!=4 || !deltaDetected || deltaBuild!=MotorBenchProtocol.BUILD_MOTOR_ID_0913){
                android.widget.Toast.makeText(this,"DeltaESC-Build 0x0913 nicht bestätigt",android.widget.Toast.LENGTH_LONG).show();return;
            }
            if(!offsetReady){
                android.widget.Toast.makeText(this,"Zuerst ADC-Offset E0 kalibrieren",android.widget.Toast.LENGTH_LONG).show();return;
            }
            if(!ble.canAcceptFrame()){
                android.widget.Toast.makeText(this,"BLE belegt: erneut starten",android.widget.Toast.LENGTH_SHORT).show();return;
            }
            new android.app.AlertDialog.Builder(this)
                .setTitle("Motor Detect R / L / Flux")
                .setMessage("Rad frei? Niemand am Rad? Test kann Motor und Brücke ansteuern. Bremsen oder E6 stoppen.")
                .setNegativeButton("Abbrechen",(d,w)->{})
                .setPositiveButton("E7 senden",(d,w)->{
                    if(stage==4 && deltaDetected &&
                       deltaBuild==MotorBenchProtocol.BUILD_MOTOR_ID_0913 &&
                       offsetReady && ble.canAcceptFrame()){
                        ble.write(MotorIdBenchProtocol.start());
                    }
                }).show();
        });
        root.addView(start);
        android.widget.Button stop=new android.widget.Button(this);
        stop.setText("SOFORT STOP (E6)");stop.setAllCaps(false);
        stop.setOnClickListener(clicked -> emergencyStop());
        root.addView(stop);
        android.widget.Button status=new android.widget.Button(this);
        status.setText("Motor Detect Status F7 / F8 / F9");status.setAllCaps(false);
        status.setOnClickListener(clicked -> {
            if(stage!=4 || !deltaDetected)return;
            int[] regs={0xF7,0xF8,0xF9};
            for(int idx=0;idx<regs.length;idx++){
                final int reg=regs[idx];
                handler.postDelayed(()->{
                    if(stage==4 && ble.canAcceptFrame())
                        ble.write(MotorIdBenchProtocol.status(reg));
                }, idx*350L);
            }
        });
        root.addView(status);
    }
'''
if not s.rstrip().endswith('}'):raise RuntimeError('truncated MainActivity')
s=s.rstrip()[:-1]+control+'\n}\n'
main.write_text(s)
(j/'MotorIdBenchProtocol.java').write_text('''package de.deltaesc.tool;
/** Only authenticated Ninebot-frame commands. BLE/Crypto are untouched. */
final class MotorIdBenchProtocol {
  private MotorIdBenchProtocol(){}
  static final int BUILD=0x0913, START=0xE7, STOP=0xE6;
  static byte[] start(){return NinebotProtocol.write(START,new byte[]{(byte)0xDE,(byte)0xC0,0x49,0x4D});}
  static byte[] stop(){return NinebotProtocol.write(STOP,new byte[0]);}
  static byte[] status(int reg){if(reg<0xF7||reg>0xF9)throw new IllegalArgumentException("status page");return NinebotProtocol.read(reg,16);}
}
''')
g=r/'app/build.gradle'
s=g.read_text().replace("applicationId 'de.deltaesc.motorbench072'","applicationId 'de.deltaesc.motorbench0913'").replace('versionCode 33','versionCode 35').replace("versionName '0.3.3'","versionName '0.3.5'")
g.write_text(s)
check()
assert 'deltaBuild!=MotorBenchProtocol.BUILD_MOTOR_ID_0913' in main.read_text()
assert 'setContentView(buildUi());addMotorIdBenchControls();' in main.read_text()
assert "BUILD_MOTOR_ID_0913 = 0x0913" in proto.read_text()
assert 'MotorIdBenchProtocol.start()' in main.read_text()
print('PASS v0.3.5 G30 transport frozen, E7/E6/F7-F9 appended, exact FW build 0x0913')
