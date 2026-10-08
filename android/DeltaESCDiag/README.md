# DeltaESC Diag 0.4.1

Minimal read-only Android bring-up app for DeltaESC-G30D.

- Uses the stock G30 dashboard BLE/NUS service.
- Requires the legacy NinebotCrypto 5B/5C/5D authentication flow.
- Sends DeltaESC private CMD `0x7D`.
- `ARG 0x00`: HELLO.
- `ARG 0x20`: 54-byte read-only diagnostic snapshot.
- Polls diagnostics every 500 ms after a valid HELLO.
- Contains no ARM, motor-start, config-write, detection or flash command.

The first hardware test should use the **sync-safe** firmware image. ST-Link remains the recovery/flash path; Bluetooth is only the telemetry path in v0.4.1.

The app intentionally reports phase-current information as ADC counts. Ampere conversion is deferred until the real G30D current-sense scaling is measured.
