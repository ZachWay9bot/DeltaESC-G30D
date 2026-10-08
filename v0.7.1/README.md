# DeltaESC G30D v0.7.1 — ST-Link sensorless bench

Status: **ACTIVE MOTOR BENCH CANDIDATE. ST-Link only. Not a riding release.**

This branch returns focus to the sensorless motor-control path. It is intentionally independent of SHU/Bluetooth recovery work.

Highlights:

- Active build only: `POWER_STAGE_ARM_ALLOWED=1`, `SENSORLESS_RUN_ALLOWED=1`.
- Current PI fixed so useful low-current errors no longer quantize to zero.
- Mxlemming-style flux observer with corrected alpha/beta phase convention.
- d-axis ALIGN, open-loop electrical-speed ramp, phase+speed qualified handover, then closed-loop sensorless.
- Dashboard throttle is capped to 0.4–1.5 A provisional phase-current request.
- Brake, throttle release, stale dashboard, current trip, observer loss or control-loop overrun force PB1 low and disable all six TIM1 gate outputs.
- No regenerative braking in this bench.
- `g_sensorless_bench` provides a stable ST-Link SRAM telemetry snapshot.

Default motor parameters are only startup estimates: R=0.100 ohm, L=100 uH, flux=0.012 Wb. They are not claimed measurements of the exact motor.

The host plant matrix covers 81 combinations of R/L/flux/inertia around those defaults and must reach CLOSED without runaway before CI exports a binary.

First real test: 10S, wheel completely free, low throttle only. Phase-current channel polarity/order remains the first hardware unknown.
