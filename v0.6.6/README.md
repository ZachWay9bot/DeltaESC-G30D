# DeltaESC G30D v0.6.6 - stock dashboard runtime + staged IAP

Status: **CI/development candidate. Do not flash until the v0.1.0 BLE app compatibility check and first-controller SHU procedure are explicitly approved.**

Based on the CI-green v0.6.5 staged-IAP/128-byte/VTOR candidate.

## v0.6.6 delta

- Keeps the v0.6.5 staged IAP implementation, 128-byte data blocks, encrypted-only SHU packaging and explicit VTOR relocation.
- Adds the native G30 dashboard runtime on PA2 without changing the Android/NinebotCrypto transport.
- Native `5A A5` dashboard `0x65` frames from `0x21 -> 0x20` are decoded read-only. Throttle is payload byte 1, brake payload byte 2, matching the established G30 VESC/SmartESC integrations.
- Native dashboard `0x64` requests receive a stock-shaped `0x64` ESC status frame with six bytes: mode, battery, light, beep, speed, fault.
- The first-test SYNC-SAFE payload deliberately reports Drive, 50% placeholder battery, light off, beep off, speed 0, fault 0. The 50% value is a bench placeholder because bus-voltage calibration remains deferred.
- Dashboard throttle/brake values are diagnostic only in this step. They do not request torque.
- D0-D9 diagnostics and E0-E6/F0-F5 protocol remain present; build identity is `0x0606`.
- App-originated reads no longer falsely count as dashboard-runtime traffic; the dashboard-seen flag now requires native source `0x21`.
- The proven G30 Bench BLE startup sequence is preserved: ESC `0x1A` returns build `0x0606`, ESC `0x10` returns the controller's own 14-byte stock serial scanned read-only from preserved configuration flash, then D0 exposes the `DESC` identity. No user-specific serial is hard-coded.

## Safety boundary

The intended first hardware image remains SYNC-SAFE: power-stage arming and sensorless run are compiled out. v0.6.6 is meant to validate the complete stock dashboard/BLE/ESC communication loop and later SHU reflash path before any torque-producing test.
