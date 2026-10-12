#!/usr/bin/env python3
"""Retarget existing v0.3.7 read-only ADC proof UI from 0x0918 to 0x0919.

Never modify the proven G30 BLE/crypto transport. No new motor commands.
Run after android/v0.3.7/apply_v037.py against the reconstructed app source.
"""
from pathlib import Path
import hashlib,sys
root=Path(sys.argv[1]).resolve()
java=root/'app/src/main/java/de/deltaesc/tool'
pin={
'BleUartClient.java':'3bbf0a9e06dd277ef30cecae9baf579f8f843b0e5db505853cfe78c916b14485',
'NinebotCrypto.java':'57d88389891ad2fe5dc42dc69748c2a81dc24edc3930081a74ceee45ae035705',
'NinebotProtocol.java':'866fb803a439faf78ce50655cf54cae15a4467766826088e0bfa86b8468e84a3'
}
def once(s,a,b):
    if s.count(a)!=1:raise RuntimeError('v0.3.8 source drift: '+repr(a))
    return s.replace(a,b,1)
def verify():
    for name,h in pin.items():
        assert hashlib.sha256((java/name).read_bytes()).hexdigest()==h,name
verify()
main=java/'MainActivity.java'
s=main.read_text()
s=once(s,'DeltaESC 0x0918 · ADC DIAGNOSE (NUR LESEN)',
       'DeltaESC 0x0919 · ADC DIAGNOSE (NUR LESEN)')
s=once(s,'Nur für angemeldete DeltaESC Firmware 0x0918; kein Zugriff auf Stock.',
       'Nur für angemeldete DeltaESC Firmware 0x0919; kein Zugriff auf Stock.')
assert 'sendEncrypted(MotorIdBenchProtocol.start()' not in s
assert 'addMotorIdBenchControls()' not in s
assert 'sendEncrypted(AdcProofTelemetry.read(reg)' in s
main.write_text(s)
p=java/'AdcProofTelemetry.java'
s=p.read_text()
s=once(s,'BUILD=0x0918','BUILD=0x0919')
s=once(s,'G30D DeltaESC v0.9.18','G30D DeltaESC v0.9.19')
# The existing v0.3.7 decoder kept a stale protocol error even after a
# valid retry. Reset it only AFTER full 18-byte/version validation succeeds.
s=once(s,'        byte[] p=Arrays.copyOf(data,LENGTH);',
       '        error="";\n        byte[] p=Arrays.copyOf(data,LENGTH);')
p.write_text(s)
g=root/'app/build.gradle'
s=g.read_text()
s=once(s,"applicationId 'de.deltaesc.adcproof0918'","applicationId 'de.deltaesc.adcproof0919'")
s=once(s,'versionCode 37','versionCode 38')
s=once(s,"versionName '0.3.7'","versionName '0.3.8'")
g.write_text(s)
verify()
print('PASS: read-only ADC FA/FB matches 0x0919; G30 crypto hash unchanged; no E7 start')
