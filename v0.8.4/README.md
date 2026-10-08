# DeltaESC G30D v0.8.4: shared motor configuration contract

**SOURCE ONLY. Hardware PWM gates remain compile-disabled. Not for flashing or riding.**

v0.8.4 resolves a real fork conflict: the advanced DRV126 motor branch had
an ADC1/ADC2 sector-dependent current-sensor model and commissioning blockers,
but its F0/F1/F2 writes still updated the motor model individually. The
separate v0.8.1 transaction branch avoided partial observer changes, but did
not contain the newer stock-current architecture or commissioning mask.

This merge brings both into one source candidate, retaining:
- Original stock G30 dashboard via PA2/USART2 and NinebotCrypto, with
  DashBLE Motor v0.3.0 read-only diagnostics DA..DF
- Restored DRV126 sector pairs and ADC dual injected-simultaneous **plan**
- 4 kHz sensorless FOC regulator, R/L/flux BEMF observer, start/handover
  state machine (not authorized to drive physical phases)
- D9 16-byte commissioning blocker report with the three deliberately
  invalid hardware qualification bits
- STM32 PA12 power button, PA11 power hold, staged IAP / retry fixes

## Transaction protocol (new build 0x0804)

Only after NinebotCrypto authentication and D0 `DESC`, build exactly
`0x0804`. Every WRITE is `0x03`; a successful stage ACK is status `6`
(`ACT_RAM_ONLY`), not status 0. All writes must be performed while motor
STOP, power stage disarmed, E0 offset calibration inactive, IAP inactive,
and poweroff not pending.

| Register | Write payload | Role |
|---|---|---|
| F0 | LE16 magic C0DE + LE32 R in micro-ohms | stage resistance |
| F1 | LE16 magic C0DE + LE32 L in nanohenry | stage inductance |
| F2 | LE16 magic C0DE + LE32 Flux in microweber | stage flux |
| F3 | LE16 magic C0DE + signed LE16 electrical angle offset | stage phase offset |
| F4 | LE16 magic C0DE + LE16 current in mA | stage test current |
| F5 | LE16 magic C0DE | commit all five fields atomically |
| F5 | LE16 magic C0DE + 00 | abort partial staging |

Read-only `E7` (16 bytes): byte0 staged mask (31 = all five present),
byte1 active-valid, byte2 motor model valid, byte3 gate armed
(must be zero), bytes4-5 active test current mA, bytes6-7 commissioning
blocker mask, bytes8-11 active R (uOhm), bytes12-15 active L (nH).

D3-D5 continue to return **active** R/L/flux only. Intermediate F0..F4
values do not change them or the running observer. No STM32 flash
configuration persistence, no E5 motor drive permission.

## Android compatibility

The previously phone-tested DashBLE Motor v0.3.0 is **read-only** and can
identify build 0x0804 and read DA..DF. DashBLE Config v0.3.1 is
**intentionally locked** to build 0x0800 and cannot configure 0x0804.
A subsequent Android app must implement all five staged fields, F5 commit
and readback, E7 pending/active status, with explicit user confirmation.
Do not broaden the old app build check or simply force F0-F2.

## Hardware limit

Current source code models the DRV126 sector-dependent dual injected ADCs,
but does **not** yet switch the physical ISR from the older four-rank
ADC1 injected readout. PWM polarity, driver enable/BKIN, ADC timing,
current polarity/gain and motor R/L/flux are not yet measured on the user's
controller. E1 and E5 stay software/hardware blocked. A CI-green build
cannot prove the bridge is safe to energize.

ST-Link remains for **recovery only**; ordinary diagnostics and future
parameter writes must use the known-good phone/dashboard Bluetooth path.
