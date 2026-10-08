# DeltaESC-G30D

> [!CAUTION]
> ## DEVELOPMENT ONLY — NO HARDWARE FLASH RELEASE
>
> This is an experimental sensorless FOC firmware for the original Ninebot G30D Gen1 STM32F103 3-cap ESC.
> **Do not flash a development SHU ZIP to the valuable stock controller yet.**
> No end-to-end Bluetooth installation + confirmed Bluetooth rollback or torque-producing motor run has been proven on this exact controller. CI success is software verification, not hardware validation.

## Canonical development references (2026-10-08)

The default `main` branch is a **repository index**, not the latest firmware. Do not select firmware by looking at the default branch or by choosing the highest version number alone.

| Version / branch | State | Purpose |
|---|---|---|
| [v0.6.9 PA12 + IAP source audit](https://github.com/ZachWay9bot/DeltaESC-G30D/tree/freeze/deltaesc-g30d-v0.6.9-pa12-iap-source-ci-green-2026-10-08) | **CI green, SOURCE-ONLY, NOT FOR FLASH** | Combines v0.6.8 staged-update guard with PA12 long-press, PA11 power-hold, stock 0x79 power-off and write ACK handling; retains v0.1.x app identity and G30 dashboard runtime. [CI run](https://github.com/ZachWay9bot/DeltaESC-G30D/actions/runs/37836783098) | 
| [v0.6.8 stock bootloader audit](https://github.com/ZachWay9bot/DeltaESC-G30D/tree/freeze/deltaesc-g30d-v0.6.8-iap-copy-audit-ci-green-2026-10-08) | **CI green, SOURCE-ONLY** | Verified critical DRV126 erase-error counterexample: bootloader may clear pending update after app pages are erased. No guaranteed Bluetooth-only recovery. |
| [v0.6.7 v0.1.x app compatibility](https://github.com/ZachWay9bot/DeltaESC-G30D/tree/freeze/deltaesc-g30d-v0.6.7-v01-app-compat-ci-green-2026-10-08) | **CI green, hardware unvalidated, superseded for new testing** | Earlier **SYNC-SAFE** baseline; v0.6.8 audit identified an IAP hazard. Adds explicit 14-byte `0x10` ESC identity for the hardware-proven G30 Bench BLE v0.1.x app |
| [v0.6.6 dashboard runtime](https://github.com/ZachWay9bot/DeltaESC-G30D/tree/freeze/deltaesc-g30d-v0.6.6-dashboard-runtime-ci-green-2026-10-08) | CI green, hardware unvalidated | Native G30 `0x64/0x65` dashboard traffic on PA2, read-only throttle/brake, fixed 50% battery status placeholder |
| [v0.6.6 dashboard-drive RC](https://github.com/ZachWay9bot/DeltaESC-G30D/tree/freeze/deltaesc-g30d-v0.6.6-dashboard-rc-ci-green-2026-10-08) | CI green, **NOT for first flash or road use** | Optional low-current torque-capable laboratory experiment; hardware current/phase measurements are still missing |
| v0.6.5 | CI green, superseded by v0.6.7 | 128-byte staged-IAP, final-block padding and VTOR |
| v0.4.2 | **RETRACTED — DO NOT FLASH** | Obsolete vector-invalidation handoff based on an unproven recovery assumption |

### Stable BLE tool

The G30 Bench BLE **v0.1.0 and v0.1.1** application family has been physically tested on stock G30D BLE dashboard `NBScooter2088` with MIC-verified legacy NinebotCrypto authentication and a working ESC read route:
- `CMD 0x01 / reg 0x1A`: stock version word `0x0420`;
- `CMD 0x01 / reg 0x10`: stock 14-byte controller serial;
- `CMD 0x01 / reg 0xD0`: stock/unknown response **not** recognized as DeltaESC.

The latest v0.6.7 firmware intentionally replies to `0x10` with 14-byte ASCII `DELTAESC-G30D0`, and to `0xD0` with `DESC`, protocol 0.2, build `0x0607`. This provides an application identity without changing the known-good Android BLE/GATT/NinebotCrypto stack.

The older v0.2ci/link-probe app connection regression must **not** be used as a reason to replace the proven v0.1.x transport again.

## Safety and update policy

- **Bluetooth-first**: intended normal installation and return-to-stock path uses SHU/stock dashboard/Ninebot IAP. ST-Link is emergency recovery only, not the routine test procedure.
- **SYNC-SAFE only for a first hardware communication test**: the power-stage arming and sensorless run are compiled out.
- Stock 4 KiB bootloader remains `0x08000000..0x08000FFF`; app base `0x08001000`.
- The v0.6.7 staged-IAP path uses the upper-flash staging area, an update control block, 128-byte packet handling, stock IAP ACK, external checksum and NinebotTEA checksum.
- Its **actual** rollback operation through the preserved bootloader has **not** yet been physically validated on this specific ESC. Do not mistake implementation plus CI for proof of recovery.
- First electrical tests are on **10S** only. Battery-divider mapping, BMS data, phase-current channel polarity/order, R/L/flux, observer handover, current-loop tuning and 14S operation remain unqualified.
- Dashboard drive RC is a lab experiment, not a road firmware. Its nominal current cap is not a verified current limit until ADC/shunt calibration succeeds.

## Relevant successful CI runs

- [v0.6.7 v0.1.x app compatible, success](https://github.com/ZachWay9bot/DeltaESC-G30D/actions/runs/37802381173)
- [v0.6.6 stock dashboard runtime, success](https://github.com/ZachWay9bot/DeltaESC-G30D/actions/runs/37801003694)
- [v0.6.6 dashboard drive RC after patch repair, success](https://github.com/ZachWay9bot/DeltaESC-G30D/actions/runs/37803370548)

**Next release gate**: remain on stock firmware with the valuable 3-cap ESC. The v0.6.8 reverse engineering found a real stock-bootloader erase-error path that clears pending IAP state while the active app is invalid. That can defeat Bluetooth/SHU recovery irrespective of an app-side patch. The v0.6.9 CI-green source-only integration does not resolve that protected-bootloader failure path and must not be flashed. Independent recovery verification on noncritical hardware is required before reconsidering a physical test; ST-Link is emergency recovery, not the proposed normal update path.

## License

GPL-3.0. Experimental source and CI artifacts are published for inspection; none imply approval for road use.
