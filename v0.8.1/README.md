# DeltaESC G30D v0.8.1 — DashBLE commissioning guard

**SOURCE-ONLY SOFTWARE CANDIDATE. HARDWARE GATES COMPILE-DISABLED. NOT A FLASH OR ROAD RELEASE.**

This revision continues from the CI-green v0.8.0 sensorless + DashBLE integration and keeps the normal access contract unchanged:

**Android DashBLE -> original Ninebot Dashboard BLE/NinebotCrypto -> yellow PA2 USART2 -> ESC. ST-Link remains recovery only.**

## v0.8.1 delta

The sensorless motor core itself is not loosened. Instead, v0.8.1 adds the commissioning interlock that a later active build must pass before E5 can ever arm the bridge.

- New `commissioning_guard.c/.h` evaluates all run blockers as one 16-bit mask.
- `E1` becomes a non-driving commissioning preflight command. It never arms PWM. It ACKs `OK` only if the entire software/hardware qualification mask is clear.
- Existing `E5` active path is wired to the same guard before zero-vector arm. In this source-only build E5 still cannot arm because `POWER_STAGE_ARM_ALLOWED=0` and `SENSORLESS_RUN_ALLOWED=0`.
- `D9` remains backward compatible in bytes 0..5 and expands to 16 bytes with the exact commissioning blocker mask, offset-valid, motor-parameter-valid, phase-map-valid, last action status, raw VBUS ADC and requested test current.
- A completed explicit `E0` offset calibration now sets a separate `offset_valid` qualification bit.
- The guard checks ADC freshness, idle phase current, explicit offset calibration, R/L/flux validity, dashboard freshness, idle throttle/brake, safety latch, IAP activity, pending shutdown, stopped control state and raw VBUS sanity.
- Three hardware facts remain deliberately **false** and therefore keep E1/E5 blocked: phase-current scale/polarity qualification, six-gate/gate-enable hardware qualification, and real ADC current-sample timing qualification.

Those last three blockers are intentional. Green CI must not magically promote assumptions about this original 3-cap ESC into motor permission.

## D9 extension

Little-endian:

| Offset | Field |
|---|---|
| 0 | current fault `u16` |
| 2 | existing diagnostic flags `u16` |
| 4 | hard over-current counts `u16` |
| 6 | commissioning guard mask `u16` |
| 8 | explicit E0 offset-valid `u8` |
| 9 | R/L/flux model-valid `u8` |
| 10 | phase-map-valid `u8` |
| 11 | last action status `u8` |
| 12 | raw VBUS ADC `u16` |
| 14 | requested test current mA `u16` |

Hardware qualification bits in the guard are `0x1000`, `0x2000`, `0x4000`. They are hard-blocked in v0.8.1.

## Safety boundary

No SHU ZIP and no flashable firmware image is exported by CI. The only target build is still gate-OFF SYNC-SAFE. Existing v0.6.8-v0.6.10 IAP safety and retry work, PA12 power button, PA11 hold, stock dashboard runtime, NinebotCrypto transport and v0.8.0 R/L/flux sensorless observer are retained.

Next firmware work is to replace the three hardware blocker bits with evidence, not guesses: current-sense scale/polarity/order, gate polarity/enable/BKIN behavior and actual ADC sample timing. Until then the phone can configure/read the model and inspect why motor permission is denied, but cannot make the bridge drive.
