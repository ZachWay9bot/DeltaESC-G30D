# DashBLE ADC v0.3.3 — read-only v0.8.5 E8 dual-ADC diagnostics

**Android diagnostics source/CI build, not a motor firmware release.** Preserves all phone-tested v0.3.0/v0.3.2 NinebotCrypto/BLE transport classes byte-for-byte.

## Distinct compatibility interlocks

- **Read-only E8** is available only after NinebotCrypto pairing, D0 contains the `DESC` signature, and `deltaBuild == 0x0805`.
- **Motor configuration writes** remain restricted to `deltaBuild == 0x0804` and the proven five-field F0–F5 transaction. No new write support for firmware `0x0805`. A version never implies compatibility automatically.
- With the owner's known stock ESC `0x0907` (and `deltaesc_detected=false`), **both features stay locked**.
- Source hashes for `BleUartClient.java`, `NinebotCrypto.java`, `NinebotProtocol.java` are checked against v0.3.2 before and after the overlay. Their behavior and the 5B/5C/5D authentication are unchanged.
- Separate installation identity `de.deltaesc.adcdiagnostics`; original v0.3.0 and v0.3.2 apps remain installed.

## The 16-byte E8 register schema from firmware v0.8.5

| Byte offset | Little-endian value |
|---|---|
| 0–1 | latest injected ADC1 JDR1 raw 12-bit count |
| 2–3 | latest injected ADC2 JDR1 raw 12-bit count |
| 4–5 | most recent ADC1 bus-voltage sample, raw ADC counts (PA1) |
| 6–7 | elapsed milliseconds since the last VBUS sample (modulo 65536) |
| 8 | just-completed acquisition sector 1–6 |
| 9 | ADC1 injected channel number |
| 10 | ADC2 injected channel number |
| 11 | bit0: offset calibration active; bit1: offset valid; bit2: VBUS read pending |
| 12–13 | VBUS conversion timeout-event counter |
| 14–15 | ADC2 injected-sequence JSQR low 16 bits |

Channel pairs: sectors 1/6 = channels 4,5; sectors 2/3 = 3,5; sectors 4/5 = 3,4. The UI marks unknown sector/channel combinations as **unconfirmed**, not as valid readings. No conversion to amps or volts is performed because current-polarity/gain and PA1 voltage scale still need physical validation.

The user interface provides an individual E8 read, an E8 auto-read every 3 seconds, Stop, a text display and JSON export containing `e8_protocol`, `e8_complete` and `e8_raw`.

Automatic polling halts on unrecognized firmware, BLE queue rejection, malformed response, connection loss or 1.8-second timeout; while running, motor diagnostic reads and configuration are blocked to avoid contention. Both read and write operations use the unchanged NUS/NinebotCrypto transport.

## Proven boundaries

- The E8 format was verified against the v0.8.5 firmware source and synthetic ARM Ninebot E8 READ-ACK responses. Host tests cover all six sectors, length rejection, little-endian fields, VBUS and calibration flags, version guards and JSON.
- **E8 is not yet tested on a flashed real G30D ESC.** The source-only v0.8.5 ESC image still has physical gates compile-disabled; PWM polarities, shunt gain/polarity and real ADC timing remain unqualified.
- This APK is for subsequent controlled hardware diagnosis *after* an independently validated recovery/installation procedure. **Do not flash v0.8.5 just to make the screen show data**.
- **ST-Link exclusively for recovery; smartphone DashBLE for configuration and diagnostics.**

## Reproduce

The CI `.github/workflows/android-v033-e8-dual-adc.yml` reconstructs the previously tested v0.3.2 Android source through the original overlay sequence, checks the SHA-256 `bb9090fd64f7296b67ab702ed746cf5a248a6a5baf1087e2bab97aa672703893` of the new E8 overlay and runs its `apply_v033.py`. It verifies the exact G30 transport hashes, compiles the Java helper tests and Android APK, and publishes the APK plus the complete editable application sources as a CI artifact.
