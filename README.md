# DeltaESC-G30D

> [!CAUTION]
> ## EXPERIMENTAL / UNVALIDATED MOTOR-CONTROL FIRMWARE
>
> This repository contains hardware bring-up firmware for the Ninebot G30D Gen1 STM32F103 ESC.
> It is **not validated for road use**. Use only with ST-Link/SWD recovery available and a verified
> full 128 KiB controller backup.
>
> Current builds are intended for bench diagnostics and staged power-stage validation only.

## Project direction

DeltaESC-G30D is a clean sensorless FOC bring-up for the stock G30D Gen1 STM32F103 controller.
It is intentionally separated from the older SmartESC/DeltaESC development tree.

Current rules:

- sensorless control, no Hall sensors required;
- ST-Link/SWD only during bring-up;
- SHU/OTA packaging is deferred until motor control is proven;
- first tests use a 10S battery;
- 14S bus-voltage scaling is deferred;
- every stage must pass before torque-producing operation is enabled.

## Current milestones

- **v0.3**: passive 4 kHz diagnostic/control-path benchmark, physical bridge disabled.
- **v0.4**: TIM1 complementary-PWM and ADC synchronization bring-up.
  - `syncsafe` build cannot arm the bridge.
  - `zero_vector_active` build can be manually armed for oscilloscope validation only and commands equal phase duty, not a rotating torque vector.
- **v0.5 development**: EBiCS/VESC-style nonlinear flux observer plus guarded
  `STOP -> ALIGN -> OPEN -> HANDOVER -> CLOSED` sensorless state machine.
  Three build variants separate sync-safe diagnostics, zero-vector arming and
  the explicit low-energy sensorless bench candidate.

v0.5 still requires real G30 motor R/L/flux identification, current sign/phase
verification and bus-voltage calibration before sensorless handover is considered
hardware-validated. See `docs/V0_5_SENSORLESS_BRINGUP.md`.

## Flash layout

- Bootloader/IAP remains at `0x08000000..0x08000FFF`
- DeltaESC application base: `0x08001000`
- Current executable ceiling: `0x0800D800`
- Stock OTA/config areas are intentionally left outside the active development image

## License and references

This project is released under GPL-3.0. Sensorless observer architecture is informed by the open-source VESC firmware; hardware mapping is cross-checked against public G30 reverse-engineering work. See the version documentation for specific references.

**Do not treat a successful build as hardware validation.**
