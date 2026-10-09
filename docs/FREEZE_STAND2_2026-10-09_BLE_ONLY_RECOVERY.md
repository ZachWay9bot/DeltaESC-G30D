# Freeze Stand 2 — BLE-only recovery path — 2026-10-09

This freeze supersedes the recovery-path wording of Stand 1.

## Mandatory recovery architecture

The intended and required normal recovery/update path is:

**Android phone -> BLE -> stock Ninebot dashboard/BLE module -> ESC UART -> ESC application IAP -> preserved stock bootloader**

The project must **not** depend on ST-Link for normal installation, update, rollback, or validation.

ST-Link is permitted only as an emergency last-resort recovery method if the BLE/IAP path is already lost. It is not an acceptable substitute for proving the OTA path.

## Current real-controller state

- Real hardware: Ninebot G30D Gen1 original 3-cap ESC.
- DeltaESC v0.6.5 SYNC-SAFE is currently installed and boots.
- Motor power stage remains disabled in the SAFE build.
- Power-management behavior in v0.6.5 is incorrect: PA11 is held high while stock shutdown behavior is incomplete.
- Physical power button does not currently shut the scooter down.
- SHU can reconnect intermittently over BLE.
- SHU initially identifies the ESC as `max_drv_unknown`.

## Board-type compatibility test

A stock DRV126 recovery package was prepared with the verified stock DRV126 encrypted payload unchanged and the package compatibility list expanded for `max_drv_unknown`.

Observed on real Android/SHU:

1. SHU accepts the patched package.
2. SHU displays it as `max / DRV`.
3. User can reach the flash confirmation screen.
4. After starting the flash, SHU stays at `INITIALIZING / Preparing to flash...`.
5. SHU times out before transfer progress starts.

Therefore the board-type patch fixes only package-selection compatibility. It does **not** yet prove or repair the BLE IAP-entry sequence.

## Frozen engineering objective

The next firmware/protocol task is to make the following cycle work using the phone only:

**Stock -> DeltaESC SAFE -> Stock DRV126**

All three phases must complete through BLE/SHU without ST-Link:

1. SHU can identify the controller sufficiently to accept the package.
2. SHU can enter IAP mode and receive the expected response.
3. 128-byte firmware blocks transfer successfully.
4. CRC/commit succeeds.
5. Stock bootloader copies staged image after reset.
6. Stock DRV126 boots.
7. SHU again reads normal Max/DRV identity and version.

## Current suspected fault area

The timeout occurs before visible firmware block transfer, so investigation remains focused on the exact SHU/Ninebot IAP entry handshake and response framing.

The current `0x0B` ESC-side ACK assumption is not yet accepted as sufficient proof for SHU-over-BLE behavior.

This remains a hypothesis until matched against the exact BLE/SHU transaction sequence.

## v0.6.6 status

The v0.6.6 development fix already corrects the discovered power-management and dashboard pin mistakes:

- PA2 / yellow = dashboard UART.
- PA12 / green = physical power-button input.
- PA11 = power hold.
- Ninebot write semantics corrected.
- SAFE motor gating remains disabled.

v0.6.6 must not be flashed to this controller until the BLE-only Stock rollback path is proven.

## Freeze decision

**Stand 2: BLE/phone recovery is mandatory. No planned ST-Link step.**

Next deliverable is a corrected IAP entry/ACK implementation or protocol proof that allows SHU on Android to start and complete a stock DRV126 rollback over BLE.
