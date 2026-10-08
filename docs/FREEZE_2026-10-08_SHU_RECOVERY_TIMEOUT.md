# Freeze: real-hardware SHU recovery timeout — 2026-10-08

This freeze records the exact real-controller state after the first DeltaESC v0.6.5 SYNC-SAFE hardware test.

## Hardware result

- DeltaESC v0.6.5 SYNC-SAFE boots on the real Ninebot G30D Gen1 3-cap ESC.
- The scooter remained powered and the physical power button no longer shut it down.
- Root cause found in v0.6.5: PA11 was held HIGH while stock power-button/power-management behavior was not implemented.
- BLE/SHU could reconnect intermittently, so the controller was not electrically dead.
- SHU identified the ESC as `max_drv_unknown`.

## Stock recovery package result

A stock DRV126 recovery ZIP was made with the original verified DRV126 encrypted payload unchanged and only the SHU compatibility list expanded to include `max_drv_unknown`.

Real SHU behavior:

1. The patched package is accepted by SHU and displayed as `max / DRV`.
2. SHU reaches the flash confirmation screen.
3. After `Flash starten`, SHU remains at `INITIALIZING / Preparing to flash...`.
4. It then fails with `Timed out! Please retry flashing.`
5. There is currently no evidence that the firmware data-transfer/write phase began.

Therefore the board-type compatibility patch solves only SHU's package-selection gate. It does not solve the actual OTA/IAP entry path.

## Important conclusion

Do **not** repeatedly retry the Bluetooth flash on the valuable original controller until the IAP entry mismatch is understood.

The current evidence points to a protocol mismatch before or at IAP BEGIN/ACK, not to a bad DRV126 payload.

## IAP protocol evidence under review

The public Ninebot IAP client performs, for ESC firmware:

- repeated lock writes before entering IAP,
- `CMD_IAP_BEGIN`,
- block writes,
- CRC,
- reset.

Its receive logic differs between protocol variants. The exact SHU-over-G30-BLE behavior still needs to be matched against v0.6.5. In particular, the existing v0.6.5 assumption that the raw stock ESC-side `0x0B` IAP ACK is sufficient for SHU is **not yet hardware-proven**.

This is a working hypothesis only; it is not frozen as the root cause.

## v0.6.6 development fix already completed

Separately, v0.6.6 fixes the discovered power-management mistakes:

- PA2 / yellow = dashboard UART only.
- PA12 / green = physical power-button input.
- PA11 = power hold.
- Ninebot write semantics corrected.
- Stock-compatible identity handling added.
- SYNC-SAFE still compiles out motor arming and sensorless run.

v0.6.6 is **not** to be flashed onto this controller until stock recovery is complete and the SHU/IAP entry path is understood.

## Freeze decision

Current controller status: **recoverable-looking but Bluetooth stock reflash not yet proven**.

Next engineering task: determine the exact SHU IAP-entry transaction and response sequence and fix the OTA path before any further real-controller flash attempt.
