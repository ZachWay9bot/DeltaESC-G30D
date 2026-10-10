#!/usr/bin/env python3
"""Exact-build overlay for VESC-method G30 Motor Detect v0.9.14.
The known-good NinebotCrypto/BLE transport must remain byte-identical.
Apply only AFTER apply_v035.py on the reconstructed v0.3.5 app."""
import hashlib,sys
from pathlib import Path
root=Path(sys.argv[1]).resolve()
j=root/"app/src/main/java/de/deltaesc/tool"
pinned={
    "BleUartClient.java":"3bbf0a9e06dd277ef30cecae9baf579f8f843b0e5db505853cfe78c916b14485",
    "NinebotCrypto.java":"57d88389891ad2fe5dc42dc69748c2a81dc24edc3930081a74ceee45ae035705",
    "NinebotProtocol.java":"866fb803a439faf78ce50655cf54cae15a4467766826088e0bfa86b8468e84a3"
}
def verify():
    for name,expected in pinned.items():
        if hashlib.sha256((j/name).read_bytes()).hexdigest()!=expected:
            raise RuntimeError("Frozen paired G30 transport modified: "+name)
def replace(path,old,new):
    s=path.read_text()
    if old not in s:raise RuntimeError(f"{path}: missing {old}")
    path.write_text(s.replace(old,new))
verify()
for name in ["MainActivity.java","MotorBenchProtocol.java","MotorIdBenchProtocol.java"]:
    replace(j/name,"0913","0914")
replace(j/"MainActivity.java","v0.9.13","v0.9.14")
replace(root/"app/build.gradle","0913","0914")
replace(root/"app/build.gradle","versionCode 35","versionCode 36")
replace(root/"app/build.gradle","versionName '0.3.5'","versionName '0.3.6'")
test=root/"tools/TestMotorBenchProtocol.java"
if test.exists() and "0913" in test.read_text():
    replace(test,"0913","0914")
main=(j/"MainActivity.java").read_text()
proto=(j/"MotorBenchProtocol.java").read_text()
assert "BUILD_MOTOR_ID_0914 = 0x0914" in proto
assert "deltaBuild==MotorBenchProtocol.BUILD_MOTOR_ID_0914" in main
assert "sendEncrypted(MotorIdBenchProtocol.start()" in main
assert "sendEncrypted(MotorIdBenchProtocol.status(" in main
assert "motorIdResultText" in main
assert "// E5 old manual drive bench button omitted for 0x0914" in main
assert "emergencyStop()" in main
assert "NinebotProtocol.build" in (j/"MotorIdBenchProtocol.java").read_text()
verify()
print("PASS motor detect Android v0.3.6 (0x0914); crypto/transport SHA256 unchanged")
