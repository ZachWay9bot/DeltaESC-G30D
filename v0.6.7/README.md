# DeltaESC G30D v0.6.7 - v0.1.x app compatibility + stock dashboard runtime

Status: **development candidate; not yet a hardware flash release.**

Based on the CI-green v0.6.6 dashboard-runtime freeze.

## v0.6.7 delta

- Keeps v0.6.6 native G30 `0x64/0x65` dashboard runtime unchanged.
- Keeps the v0.6.5+ staged IAP, 128-byte transport, encrypted-only SHU packaging and explicit VTOR relocation unchanged.
- Adds stock read register `0x10` with an explicit 14-byte ASCII controller identity: `DELTAESC-G30D0`.
- This satisfies the already hardware-proven G30 Bench BLE v0.1.0/v0.1.1 ESC-route check, which reads `0x1A` then `0x10` before D0.
- D0-D9 layout remains unchanged; D0 reports `DESC`, protocol 0.2 and build `0x0607`.
- The Android BLE/GATT/NinebotCrypto path is not changed.
- SYNC-SAFE still compiles out power-stage arming and sensorless run.

The custom `0x10` identity is deliberately not copied from unknown stock flash/config locations. It identifies the running custom ESC firmware without inventing a factory serial-number address.
