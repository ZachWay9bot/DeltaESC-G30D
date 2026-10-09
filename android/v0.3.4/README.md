# DashBLE ADC Safety v0.3.4 — G30D E8 integrity companion

**Android debug APK. Bluetooth connection previously verified with
DashBLE v0.3.2 on the original Ninebot G30D; E8/v0.8.6 still awaits
testing against actual flashed controller hardware.**

Adds support for exactly two compatible read-only E8 firmware IDs:

- `DESC` build **0x0805**: original E8 passive dual ADC raw values
- `DESC` build **0x0806**: same E8 layout plus new status bits 0x08
  (integrity fault latched), 0x10 (long injection interval), 0x20
  (abnormally short interval)

On any stock/unknown firmware, including the last observed raw DRV
`0x0907`, E8 is refused. All writes continue to require the **exact
0x0804** F0–F5 atomic config version; v0.8.5 and v0.8.6 do **not**
obtain new write permission. Do not change the `MotorTxn.BUILD` check.

The BLE stack, GATT serial handling, NinebotCrypto handshake and packet
serializer source hashes are unchanged from the owner-verified link:

| File | SHA-256 |
|---|---|
| BleUartClient.java | 3bbf0a9e06dd277ef30cecae9baf579f8f843b0e5db505853cfe78c916b14485 |
| NinebotCrypto.java | 57d88389891ad2fe5dc42dc69748c2a81dc24edc3930081a74ceee45ae035705 |
| NinebotProtocol.java | 866fb803a439faf78ce50655cf54cae15a4467766826088e0bfa86b8468e84a3 |

v0.3.4 installs separately as `de.deltaesc.adcquality` and does not
replace working apps. Uses the same E8 one-shot and 3-second auto-read
plus Stop, 16-byte length validation, bus raw/age, sector/channel
diagnostics and JSON export. No EEPROM/flash command, no E5/E6 motor
action and no ST-Link for normal diagnostics.

**E9** is a new read-only firmware v0.8.6 diagnostics register but has
not yet been added as a visible screen in this APK. Its counters are
documented in the v0.8.6 firmware README. Do not misrepresent E8
timing counts as verified phase-current amps or motor safety.

Next hardware milestone: a controlled, recoverable non-motor flash on
a suitable test controller, confirm ADC1/ADC2 injected timing,
current polarity/gain and phase order while bridge gates are held
OFF. Original stock controller rollback remains unvalidated. Do not
flash v0.8.6 on the valuable original G30D just for live values.

**ST-Link solely for recovery; phone and stock dashboard BLE for
configuration and diagnostic reads.**
