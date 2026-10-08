# Hardware checkpoint: DashBLE Motor v0.3.0

Date: 2026-10-08. Source revision frozen from `freeze/deltaesc-android-v0.3.0-dashble-motor-readonly-ci-green-2026-10-08`.
The installed Android application has been exercised by the scooter owner on the original G30D dashboard.

## Observations supplied by the hardware tester

| Observation | Result |
|---|---|
| Android tool identification | DashBLE Motor 0.3.0 |
| Real G30 dashboard BLE device | Seen and connected; name/serial identifiers deliberately withheld |
| NinebotCrypto pairing | **PASS** (`crypto_paired=true`) |
| MIC failures | **0** (`rx_mic_bad=0`) |
| Plain packet parse failures | **0** (`rx_plain_bad=0`) |
| ESC firmware identification read | **PASS**, reported raw value `0x0907` |
| ESC serial read | **PASS**, serial value deliberately **not recorded** |
| DeltaESC firmware signature | **NOT DETECTED** (`deltaesc_detected=false`) |
| DeltaESC build / feature flags | Not available (`delta_build=""`, `delta_flags=-1`) |
| Read-only safety of mobile tool | Reported `read_only=true`; read-only command policy independently covered by CI |
| DA–DF motor-probe readback | **NOT TESTED / NOT AVAILABLE** (`motor_complete=false`, all pages null) |
| Sensorless motor operation / phase drive | **NOT TESTED** |
| ESC firmware flash / recovery | **NOT TESTED** |

## Freeze contract

- **Do not modify** the verified BLE/NinebotCrypto source files or handshake/framing retry behavior. Preserve the verified v0.3.0 APK as a known-good test tool.
- The firmware value `0x0907` is a reported register response, **not proof of a particular stock firmware revision or DeltaESC installation**.
- DA–DF are only polled on confirmed DeltaESC build `0x0702` or newer. A null motor report is **expected** here and must not be interpreted as a motor ADC failure.
- Existing app identity `de.deltaesc.motorprobe`, versionName `0.3.0`, with standalone installation; G30 Bench BLE v0.1.0 remains available.
- **DashBLE for configuration and diagnostics; ST-Link for emergency recovery only.**
- No authorization to flash current experimental v0.7.2 ESC image to the user's original read-protected ESC. Build success does not validate power-stage behavior or bootloader recoverability.
- APK built and tested in [CI run 37848637382](https://github.com/ZachWay9bot/DeltaESC-G30D/actions/runs/37848637382).
- The Android source code, test files and deployment process are unchanged by this checkpoint; only this hardware-test note was added.

## Next work outside this freeze

1. Preserve working phone BLE credentials/framing.
2. Ensure a **safe and recoverable** installation plan exists before placing any experimental ESC application on the controller.
3. Once a compatible DeltaESC is running, validate DA–DF current-offset/noise readouts, ADC gain/sign mapping, and sensorless observer parameters with real hardware.
