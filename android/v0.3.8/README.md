# DeltaESC Android v0.3.8 – ADC proof for Motor-RC 0x0919

**Read-only telemetry app, no E7 motor launch and no SHU installer.**

Rebuilds the existing v0.3.6 Android app from pinned source, applies the
original v0.3.7 FA/FB decoder and removal of E7 start UI, then retargets
the ADC-only build guard from 0x0918 to **0x0919**.

- The frozen `BleUartClient.java`, `NinebotCrypto.java` and
  `NinebotProtocol.java` sources are verified against exact SHA-256 pins.
- Reads only pages 0xFA and 0xFB, each exactly 18 bytes. Raw ADC
  counts are explicitly not calibrated amperes.
- Rejects stock / unknown build IDs; requires verified DeltaESC signature
  plus exact build 0x0919 for ADC reads.
- Explicitly omits active E7 motor-ID UI and callback.
- Separate Android package name `de.deltaesc.adcproof0919`; will not
  replace the working G30 BLE app.
- GitHub Actions workflow:
  `.github/workflows/android-v038-adcproof-v0919.yml`.

**Important:** Motor-RC 0x0919 is an unvalidated experimental firmware
branch; this Android app being buildable is NOT authorization to flash
any experimental image. The original controller should remain unchanged.
