# DeltaESC G30D v0.6 Stock-Dash BLE

**UNVALIDATED HARDWARE TEST BUILD. Do not treat this as road-ready firmware.**

This branch is based on the CI-green v0.5 sensorless bring-up commit and adds the stock G30 dashboard link needed by the DeltaESC Android test app.

- PA2 / USART2, 115200 baud, half-duplex, AF open-drain.
- Dashboard traffic 0x64/0x65 remains serviced.
- Android/Ninebot custom reads D0-D9.
- E0 current-offset calibration works with bridge disarmed.
- E5 behavior depends on build: disabled / zero-vector only / sensorless bench.
- E6 is unconditional STOP + bridge disarm.
- F0-F4 are range-checked RAM settings.
- E1-E4 active phase/R/L/flux identification remain intentionally locked until zero-vector PWM/polarity is bench-validated.
- F5 does not write flash yet; it reports RAM-only.

The source tarball contains the complete v0.6 source, linker script, Makefile and preflight used by CI.
