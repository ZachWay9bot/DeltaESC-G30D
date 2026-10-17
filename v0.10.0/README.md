# DeltaESC G30D v0.10.0 – XBOT FO69/FO70 integration

## Purpose

This source-only integration ports the missing sensorless bring-up pieces from the
hardware-proven XBOT FO69/FO70 line into the existing DeltaESC v0.9.20 sensorless
pipeline. It does **not** replace DeltaESC's current reconstruction, FOC/SVPWM,
Motor-ID, Ninebot transport, IAP, dashboard watchdog or terminal gate-kill path.

The integration target remains non-flashable: `xbot-rolling-locked` links an ELF
only with `POWER_STAGE_ARM_ALLOWED=0`.

## What came from FO70

- passive BEMF ranking on PA6 / PA7 / PB1,
- the proven forward raw sector convention `step=5`,
- rolling speed estimate from electrical 60-degree edge timing,
- minimum rolling qualification: coherent forward edges, fresh BEMF, sufficient
  BEMF span and >= 3.0 km/h default threshold,
- a regular-ADC sequencer that averages three samples per BEMF channel and owns
  ADC1 only while the bridge is disarmed,
- read-only register `0xFC` (`RK10`) exposing rolling evidence.

Default 243 mm wheel circumference, 15 pole pairs, 40-count BEMF span and the
3.0 km/h threshold remain **calibration assumptions**, not G30 hardware proof.

## What came from FO69

- physical-forward electrical rotation is negative,
- observer phase-step plausibility gate,
- rotor-flux magnitude gate around measured lambda,
- current phase-error < 45 degrees and average error < 30 degrees,
- quiet takeover requires zero new observer clamp events,
- no forced ALIGN/OPEN fallback for rolling start,
- zero-Iq observer verification before torque,
- modest post-lock Iq ramp with a temporary 12-count cap,
- neutral -> kick -> throttle interlock and fail-closed abort behavior.

The FO69 phase-step limits were defined for 1 kHz observer phase updates. DeltaESC
runs the observer at 4 kHz, therefore the equivalent per-update limits are divided
by four (`1024` absolute, `+256` positive electrical counts).

## Already present in DeltaESC and intentionally retained

- VESC/EBiCS fixed-point flux observer,
- current PI including the earlier fractional-integrator fix,
- Clarke/Park + SVPWM,
- ADC phase-current reconstruction,
- Motor-ID R/L/flux adoption,
- dashboard brake/throttle watchdog,
- hardware-first gate-off / latched stop,
- Ninebot BLE/IAP and read-only FA/FB ADC diagnostics.

## Deliberately NOT copied from XBOT

The following are hardware-specific and must not be transplanted by numerology:

- XBOT current gain/offset values,
- XBOT current trip thresholds,
- XBOT deadtime value or gate polarity,
- XBOT AMP_LIM/BKIN assumptions,
- XBOT IAP/UART stack where DeltaESC already owns the equivalent layer.

`COMM_CURRENT_SCALE_HW_VALID` and `COMM_ADC_TIMING_HW_VALID` therefore remain `0`.

## New source modules

- `g30_rolling_bemf.[ch]` – FO70 rolling BEMF/sector/speed qualification.
- `g30_passive_bemf_adc.[ch]` – gate-off-only ADC1 regular conversion scheduler.
- `g30_rolling_start.[ch]` – standalone policy model for neutral/kick/observer/ramp.
- `sensorless_control` overlay – `ROLL_VERIFY` entry, observer seeding, FO69
  takeover checks and bounded Iq ramp.
- `motor_pipeline` overlay – neutral latch and `motor_pipeline_start_rolling()`.

## Safety / release status

**SOURCE/LINK PROOF ONLY. NO MOTOR-READY BIN.**

The first useful real-hardware validation after this merge is passive `0xFC`
rolling telemetry with the bridge off. Only after wheel geometry, pole-pair count,
sector direction and BEMF span are confirmed on the actual G30 motor should the
rolling takeover path be considered for an active current-limited bench build.
