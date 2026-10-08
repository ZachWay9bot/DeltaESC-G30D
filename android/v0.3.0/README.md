# DashBLE Motor v0.3.0 — G30D read-only motor diagnostics

Android app completing the **existing phone → stock Ninebot dashboard BLE → PA2 UART2 → DeltaESC motor diagnostics** workflow. ST-Link is **for recovery only**, not for live telemetry, configuration, debugging or calibration.

## Preserve the established Bluetooth connection

The application starts from the frozen, CI-validated v0.2.3 real-G30 link probe which was checked against frames captured from the physically working G30 Bench BLE 0.1.0 transport. The following Java files are **byte-identical** to the v0.2.3 reference:
- `BleUartClient.java` SHA-256 `3bbf0a9e06dd277ef30cecae9baf579f8f843b0e5db505853cfe78c916b14485`
- `NinebotCrypto.java` SHA-256 `57d88389891ad2fe5dc42dc69748c2a81dc24edc3930081a74ceee45ae035705`
- `NinebotProtocol.java` SHA-256 `866fb803a439faf78ce50655cf54cae15a4467766826088e0bfa86b8468e84a3`

The app is installed separately under package ID `de.deltaesc.motorprobe`. It **does not overwrite** the known-good G30 Bench BLE v0.1.0 or Link Probe apps.

## Function

1. Discover and connect to the original G30D dashboard, complete NinebotCrypto 5B/5C/5D handshake.
2. Read stock ESC firmware register 0x1A and DeltaESC D0 signature. No motor-register queries unless `DESC` and build **0x0702 or later** are confirmed.
3. Motor screen: **Once**, **Auto (every 3 seconds after a completed scan)**, **Stop**. Pages DA→DB→DC→DD→DE→DF are read *sequentially*, each as a 16-byte read-only ESC read, not fire-and-forget.
4. Show 4 raw ADC means, minima, maxima and last values, first three current offsets, sample counts, TIM1/ADC registers and control-loop cycle timings; export diagnostics as JSON to clipboard.
5. Time out if a page isn't received; stop auto polling instead of repeatedly writing after a Bluetooth failure. No ESC writes, IAP, motor-enable commands or flash action.

**Important timing limitation:** The 256-sample windows update every ~64 ms at 4 kHz. The six pages are separate 16-byte reads; they are not a strictly simultaneous snapshot. Values are marked as such, not represented as live calibrated amperes or volts. Measurement channels and ADC gains/phase signs remain unconfirmed.

**Physical testing limitation:** The CI runs the proven captured NinebotCrypto test vectors and builds Android APK but does not prove the handset or ESC has v0.7.2. A stock DRV126 ESC does **not** provide DA–DF registers. v0.7.2 ESC firmware remains source-only, not authorized for blind flashing. The app cannot make a stock controller produce diagnostics it does not implement.

## Android

- APK: `DashBLE_Motor_v0_3_0.apk`, debug signed.
- Separate app ID: `de.deltaesc.motorprobe`, versionCode 30, Android 8+.
- The package leaves pre-existing apps installed.
- No app permission for Internet is added; BLE only.

## Source

Run `android/v0.3.0/apply_v030.py /path/to/reconstructed/v023` on the verified v0.2.3 sources from `.github/workflows/android-tool-v023.yml`; the overlay refuses if any of three transport source hashes differ. Host tests run `TestMotorTelemetry.java`.
