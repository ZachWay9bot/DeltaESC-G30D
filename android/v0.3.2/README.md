# G30 Motor Bench v0.3.2 · DeltaESC v0.7.2

**Target firmware: DeltaESC G30D v0.7.2 MOTOR TEST BENCH, build `0x0720`.**
This is a separate Android app for the stock G30 BLE display; it does **not** replace the earlier v0.1.0 hardware-proven link probe or the v0.3.0 read-only app.

## Proven transport is frozen

The following files are SHA-256 checked before and after the overlay, and are not edited:
- `BleUartClient.java` `3bbf0a9e06dd277ef30cecae9baf579f8f843b0e5db505853cfe78c916b14485`
- `NinebotCrypto.java` `57d88389891ad2fe5dc42dc69748c2a81dc24edc3930081a74ceee45ae035705`
- `NinebotProtocol.java` `866fb803a439faf78ce50655cf54cae15a4467766826088e0bfa86b8468e84a3`

The known G30 dashboard / NinebotCrypto MIC transport (5B/5C/5D) stays unchanged. No SHU, IAP, or Bluetooth recovery logic.

## Function

- Read ESC version and D0–D9 motor diagnostics.
- Read F0–F4 and apply motor R/L/Flux/current settings **in RAM only**.
- E0 calibration only while disarmed.
- E5 bench request only after exact `DESC` identification with build `0x0720`, valid E0 and a clean full preflight.
- 100 to 500 mA only; **100 mA on first attempt**.
- E6 STOP always available after verified DeltaESC connection. The app requests another E6 after 1200 ms and attempts E6 when backgrounded.
- Wrong firmware, stock DRV126, missing D0, missing pages, missing calibration, stale values, current fault or phase plausibility fault must block E5.
- F5 persistence and IAP/flash commands are not available.

## IMPORTANT: firmware-side risk

This app does NOT prove that v0.7.2 has a self-contained time limit or can turn off the bridge if Android crashes or BLE disconnects. The **v0.7.6** firmware is where a 1000 ms on-controller timebox was explicitly introduced; do not confuse it with v0.7.2.

**No real ESC flashing or E5 motor test is authorized solely by passing Android CI.** First verify hardware power-hold pins and the independent motor cutoff on the intended firmware, and stage ST-Link restoration.

## Build

Workflow: `.github/workflows/android-v032-v072-motor-bench.yml`
Output after a green run:
- `G30_Bench_BLE_Motor_v0.3.2_v072.apk`
- `G30_Bench_BLE_Motor_v0.3.2_v072_SOURCE.tar.xz`
- `SHA256SUMS.txt`

The workflow reconstructs the SHA-pinned G30 v0.2.3 transport, overlays read-only v0.3.0, then existing v0.3.1 motor controls, and finally applies `android/v0.3.2/apply_v032.py`.
