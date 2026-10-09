#!/usr/bin/env python3
"""Keep original G30 BLE crypto intact; update READ-ONLY E8 to v0.8.6.
Stock/unknown firmware and all parameter WRITEs stay locked.
"""
from pathlib import Path
import hashlib,sys
root=Path(sys.argv[1]).resolve()
java=root/'app/src/main/java/de/deltaesc/tool'
expected={
    'BleUartClient.java':'3bbf0a9e06dd277ef30cecae9baf579f8f843b0e5db505853cfe78c916b14485',
    'NinebotCrypto.java':'57d88389891ad2fe5dc42dc69748c2a81dc24edc3930081a74ceee45ae035705',
    'NinebotProtocol.java':'866fb803a439faf78ce50655cf54cae15a4467766826088e0bfa86b8468e84a3'
}
def verify():
    for name,sha in expected.items():
        if hashlib.sha256((java/name).read_bytes()).hexdigest()!=sha:
            raise RuntimeError('G30 proven Bluetooth transport altered: '+name)
def one(s,old,new):
    if s.count(old)!=1:raise RuntimeError(f'expected one anchor ({s.count(old)}): {old!r}')
    return s.replace(old,new,1)
verify()
p=java/'LiveAdc.java';s=p.read_text()
s=one(s,'/** Firmware v0.8.5 E8: passive raw dual injected ADC diagnostic only. */',
      '/** Firmware v0.8.5/v0.8.6 E8 raw ADC read-only; never authorizes motor drive. */')
s=one(s,'static final int REQUIRED_BUILD = 0x0805;',
      'static final int REQUIRED_BUILD = 0x0805;\n    static final int INTEGRITY_BUILD = 0x0806;')
s=one(s,'return paired && desc && build == REQUIRED_BUILD;',
      'return paired && desc && (build == REQUIRED_BUILD || build == INTEGRITY_BUILD);')
s=one(s,'        if((flags&~7)!=0)s.append(String.format(Locale.US,"Unbekannte Status-Bits: 0x%02X\\n",flags&~7));',
'''        if((flags&8)!=0)s.append("WARNUNG: ADC-Werte/Kanalkennung ungueltig; Sperre aktiv\\n");
        if((flags&16)!=0)s.append("WARNUNG: verzoegerter ADC-Trigger (noch unkalibriert)\\n");
        if((flags&32)!=0)s.append("WARNUNG: ADC-Abtastung schneller als erwartet\\n");
        if((flags&~63)!=0)s.append(String.format(Locale.US,"Unbekannte Status-Bits: 0x%02X\\n",flags&~63));''')
s=one(s,'v0.8.5 ist Gate-OFF; keine Motorfreigabe.',
      'v0.8.5/v0.8.6 sind Gate-OFF; keine Motorfreigabe.')
p.write_text(s)
p=java/'MainActivity.java';s=p.read_text()
s=one(s,'Gesperrt: E8 nur mit DESC Build 0x0805',
      'Gesperrt: E8 nur mit DESC Build 0x0805/0x0806')
s=one(s,'Wartet auf DeltaESC 0805. Keine Ampere- oder Volt-Kalibrierung.',
      'Wartet auf DeltaESC 0805/0806. Keine Ampere- oder Volt-Kalibrierung.')
s=one(s,'DeltaESC 0805: E8 lesbar (Rohdaten)',
      'DeltaESC 0805/0806: E8 lesbar (Rohdaten)')
s=one(s,'DashBLE ADC 0.3.3','DashBLE ADC 0.3.4')
assert 'MotorTxn.permitted(stage==4,deltaDetected,deltaBuild)' in s
assert 'NinebotProtocol.readEsc(LiveAdc.REGISTER,16)' in s
assert '0xE5' not in s and '0xE6' not in s
p.write_text(s)
p=root/'app/build.gradle';s=p.read_text()
s=one(s,"applicationId 'de.deltaesc.adcdiagnostics'",
      "applicationId 'de.deltaesc.adcquality'")
s=one(s,'versionCode 33','versionCode 34')
s=one(s,"versionName '0.3.3'","versionName '0.3.4'")
p.write_text(s)
p=root/'app/src/main/AndroidManifest.xml'
s=p.read_text()
s=one(s,'android:label="DashBLE ADC"','android:label="DashBLE ADC Safety"')
p.write_text(s)
verify()
print('PASS DashBLE v0.3.4 E8 build0805/0806 reads; F0-F5 unchanged build0804-only; original G30 BLE SHA-identical')
