#!/usr/bin/env python3
"""v0.3.2 compatibility overlay for the frozen G30 0x0720 motor bench.

Run AFTER android/v0.3.1/apply_v031.py. Does not touch the transport:
BleUartClient, NinebotCrypto, NinebotProtocol are SHA-256 pinned.
"""
from pathlib import Path
import hashlib
import sys

root=Path(sys.argv[1]).resolve()
java=root/'app/src/main/java/de/deltaesc/tool'
frozen={
    'BleUartClient.java':'3bbf0a9e06dd277ef30cecae9baf579f8f843b0e5db505853cfe78c916b14485',
    'NinebotCrypto.java':'57d88389891ad2fe5dc42dc69748c2a81dc24edc3930081a74ceee45ae035705',
    'NinebotProtocol.java':'866fb803a439faf78ce50655cf54cae15a4467766826088e0bfa86b8468e84a3'
}
def check_transport():
    for name, digest in frozen.items():
        got=hashlib.sha256((java/name).read_bytes()).hexdigest()
        if got!=digest: raise RuntimeError('Frozen verified transport changed: '+name)

def change(path, old, new):
    s=path.read_text()
    if s.count(old)!=1:
        raise RuntimeError(f'{path.name}: expected unique anchor {old!r}, got {s.count(old)}')
    path.write_text(s.replace(old,new,1))

check_transport()
protocol=java/'MotorBenchProtocol.java'
main=java/'MainActivity.java'
change(protocol,'BUILD_MOTOR_BENCH_TIMEOUT = 0x0760','BUILD_MOTOR_BENCH_V072 = 0x0720')
s=main.read_text()
if 'BUILD_MOTOR_BENCH_TIMEOUT' not in s:
    raise RuntimeError('Expected v0.3.1 firmware version guard missing')
s=s.replace('BUILD_MOTOR_BENCH_TIMEOUT','BUILD_MOTOR_BENCH_V072')
# v0.7.2 does NOT prove an independent 1-second firmware cutoff.
# Remove the inherited v0.7.6 safety claim, and prioritize E6 regardless of E5 ACK.
def rewrite_once(old,new):
    global s
    if s.count(old)!=1:
        raise RuntimeError('Expected unique safety anchor: '+old[:90])
    s=s.replace(old,new,1)
rewrite_once('Firmware stoppt den Motor-Bench selbst nach 1,0 s; die App sendet zusätzlich E6.',
             'KEIN unabhängiger Firmware-STOP nachgewiesen! Handy-E6 ist NICHT ausfallsicher. Physische Abschaltung bereithalten.')
rewrite_once('PRE-FLIGHT GRÜN · E5 100–500 mA freigegeben · FW Auto-STOP 1,0 s',
             'Preflight elektrisch GRÜN · E5 nur am Rad-frei-Teststand · FW-Zeitlimit UNGEPRÜFT')
rewrite_once('Firmware stoppt nach 1,0 s automatisch; App sendet zusätzlich E6.',
             'Firmware-Zeitlimit NICHT nachgewiesen. E6 wird per Handy zeitgesteuert, bei BLE-Ausfall NICHT garantiert. Physischer Abschalter bereit?')
rewrite_once('if(pendingWrite>=0||!ble.canAcceptFrame()){handler.postDelayed(this::tryEmergencyStop,60);return;}',
             'if(pendingWrite>=0){pendingWrite=-1;pendingWriteLabel="";writeNonce++;}if(!ble.canAcceptFrame()){handler.postDelayed(this::tryEmergencyStop,60);return;}')
rewrite_once('pendingWrite=r.arg;pendingWriteLabel=r.label;final long n=++writeNonce;',
             'pendingWrite=r.arg;pendingWriteLabel=r.label;if(r.arg==0xE5)handler.postDelayed(this::emergencyStop,1200);final long n=++writeNonce;')
rewrite_once('if(f.arg==0xE5){benchState.setText("E5 akzeptiert · Motor-Bench läuft max. 1,0 s · redundantes E6 folgt");handler.postDelayed(this::emergencyStop,1200);return;}',
             'if(f.arg==0xE5){benchState.setText("E5 quittiert · E6 wurde bereits unabhängig von der Antwort geplant");return;}')
rewrite_once('hard>100||sum>30', 'hard!=100||sum!=30')
s=s.replace('0.3.1','0.3.3')
# Avoid displaying a newer firmware compatibility requirement.
s=s.replace('v0.7.6','v0.7.2').replace('0x0760','0x0720')
# Send a best-effort E6 when activity goes to background.
# NOTE: Android BLE cannot guarantee delivery after link loss.
if 'void onStop(' in s:
    raise RuntimeError('Unexpected existing onStop override, review manually')
if not s.rstrip().endswith('}'):
    raise RuntimeError('MainActivity truncated')
s=s.rstrip()[:-1]+'''
    @Override protected void onStop() {
        if (stage == 4 && deltaDetected) emergencyStop();
        super.onStop();
    }
}
'''
main.write_text(s)

grad=root/'app/build.gradle'
change(grad,"applicationId 'de.deltaesc.motorbench'","applicationId 'de.deltaesc.motorbench072'")
change(grad,'versionCode 31','versionCode 33')
change(grad,"versionName '0.3.1'","versionName '0.3.3'")

t=root/'tools/TestMotorBenchProtocol.java'
if t.exists():
    u=t.read_text().replace('BUILD_MOTOR_BENCH_TIMEOUT','BUILD_MOTOR_BENCH_V072').replace('0x0760','0x0720')
    t.write_text(u)

check_transport()
p=protocol.read_text()
m=main.read_text()
assert 'BUILD_MOTOR_BENCH_V072 = 0x0720' in p
assert 'BUILD_MOTOR_BENCH_V072' in m
assert 'deltaBuild!=MotorBenchProtocol.BUILD_MOTOR_BENCH_V072' in m
assert 'preflightOk' in m and 'offsetReady' in m
assert 'handler.postDelayed(this::emergencyStop,1200)' in m
assert 'onStop()' in m and 'emergencyStop();' in m
assert 'FW Auto-STOP' not in m and 'Firmware stoppt nach 1,0 s' not in m
assert 'if(r.arg==0xE5)handler.postDelayed(this::emergencyStop,1200)' in m
assert 'if(pendingWrite>=0){pendingWrite=-1;' in m
assert '0x0760' not in p and 'BUILD_MOTOR_BENCH_TIMEOUT' not in m
assert '0xF5' not in m and 'IAP' not in m
print('v0.3.2 v0.7.2 compatibility + safety guards PASS; frozen NinebotCrypto unchanged')
