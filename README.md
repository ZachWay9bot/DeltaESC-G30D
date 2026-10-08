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
- **v0.4.1**: stock-dashboard Bluetooth diagnostic path.
- **v0.4.2**: Bluetooth-first SHU candidate.
  - SYNC-SAFE image keeps power-stage arming compiled out;
  - ZIPv3/NinebotTEA package is built for SHU;
  - stock IAP bootloader at `0x08000000..0x08000FFF` remains untouched;
  - an exact checksum-valid IAP-start request can hand a running DeltaESC back to the stock bootloader;
  - ST-Link remains recovery-only.

The next planned stage is calibrated current sensing, phase mapping, low-energy motor parameter work and a controlled sensorless open-loop-to-observer transition.

## Flash layout

- Bootloader/IAP remains at `0x08000000..0x08000FFF`
- DeltaESC application base: `0x08001000`
- Current executable ceiling: `0x0800D800`
- Stock OTA/config areas are intentionally left outside the active development image

## License and references

This project is released under GPL-3.0. Sensorless observer architecture is informed by the open-source VESC firmware; hardware mapping is cross-checked against public G30 reverse-engineering work. See the version documentation for specific references.

**Do not treat a successful build as hardware validation.**

## Flash / recovery policy

**Bluetooth-first is the project default.** Normal installation and later firmware updates are intended to use the stock G30 BLE/dashboard path with ScooterHacking Utility and the preserved Ninebot IAP bootloader. ST-Link is recovery-only and is not part of the normal test procedure.

The preferred first-test artifact is the **v0.4.2 SYNC-SAFE SHU ZIP**, not a raw BIN. Its power-stage arming code is compiled out.

The stock-IAP handoff is deliberately strict: only an exact checksum-valid ESC update-start frame with a plausible firmware size is accepted. It then forces the bridge disarmed, requires low phase-current residual, invalidates only the upper half-word of the application stack-vector, and resets so the preserved stock bootloader remains in recovery/update mode.

**Hardware status:** packaging and static preflight are validated in CI; the IAP handoff itself is not yet proven on this exact G30D controller. ST-Link is therefore an emergency recovery option, not a normal flashing step.
