# DashBLE Config v0.3.1 — guarded motor R/L/flux configuration

**DEVELOPMENT / PHONE-APP SOFTWARE ONLY. The original stock G30D ESC cannot be configured by this app.**

## Connection
This standalone Android APK shares its entire BLE/GATT, NinebotCrypto, Ninebot frame, and MIC implementation **byte-for-byte** with the real-scooter-tested DashBLE Motor 0.3.0 (and the frozen v0.2.3 G30 connection). Install separately as `de.deltaesc.motorparams`, leaving the v0.3.0 app available.

## Writes
- Exactly F0 (R, milliohms), F1 (L, microhenries) and F2 (flux, milliwebers).
- Each field is independently range-checked in SI-scaled 32-bit units.
- **Required:** NinebotCrypto authenticated, D0 DESC signature and **exactly** build `0x0800`. Other builds including known stock raw `0x0907`, later `0x0801`/ `0x0802`, and unknown versions are locked for parameter writes.
- A user must manually press the individual **Schreiben** button and accept a confirmation dialog. Nothing is written automatically.
- The app sends exactly one Ninebot `0x03 WRITE` frame, checks `0x05 WRITE_ACK` status 0, reads back matching `D3/D4/D5` using read-only `0x01 READ`, and checks exact agreement. Any mismatch, timeout or disconnect stops the transaction.
- E0–E6 motor action commands, IAP/SHU, F3 phase mapping, F4 test current, F5 commit, and raw writes are not implemented.
- Configuration is RAM-only in v0.8.0; values are not presumed physically measured or retained after reboot.

## Important version boundary
The **newer** v0.8.1 transactional motor-configuration branch uses magic-protected *staged* fields and a separate F5 commit. This app intentionally does **NOT** claim to support that protocol. The newer v0.8.2 model also remains SOURCE-ONLY and physically unvalidated. Before combining these into a single real hardware release, update the app and firmware to a shared versioned transactional configuration contract, preserving the verified NinebotCrypto transport.

The original tested controller currently reports raw firmware value `0x0907` without a DeltaESC D0 signature. All motor-parameter writes therefore remain DISABLED. This is a deliberate safety property.

## Test
CI runs real captured G30 handshake regression vectors, host tests for motor numeric encoding/ranges/whitelisted register writes, and an Android Gradle debug build; the APK is untested on a real installed DeltaESC motor firmware. ST-Link remains recovery-only, not a calibration/debug interface.
