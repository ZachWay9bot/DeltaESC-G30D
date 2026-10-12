# DeltaESC G30D v0.9.20 – sensorless integration checkpoint

**SOURCE / CI ONLY. DO NOT FLASH. NO ROAD RELEASE.**

## Direction

The product target remains **sensorless FOC** on the original G30D Gen1
STM32F103 controller. Hall sensors are not part of the required run path and
must not become a hidden dependency while the hardware bring-up is completed.

This branch starts from the CI-green v0.9.19 Motor-RC + ADC-proof integration.
That branch already descends from the VESC/EBiCS fixed-point observer work, so
v0.9.20 does not restart the motor-control stack and does not fall back to the
old SmartESC Hall path.

The intended chain is:

```
DRV126-derived ADC/current timing
        -> phase-current reconstruction
        -> Clarke/Park + current PI
        -> SVPWM/TIM1
        -> ALIGN -> OPEN -> HANDOVER
        -> VESC/EBiCS Ortega observer CLOSED LOOP
```

## Stock DRV126 full-dump evidence

The user's 128 KiB DRV126 full dump has SHA-256:

```
9235b466a2f7449b8184560dbef9012d7c12b099e4076100a223ef68d204bb68
```

The existing read-only v0.9.8 audit matches all nine firmware anchors in that
exact dump, including:

- six-sector ADC pair dispatch;
- signed current scaling `delta * 51575 >> 10`;
- sector ADC1/ADC2 JSQR selection;
- TIM1 PSC/ARR/RCR center-aligned profile;
- CH4 PWM1 ADC trigger;
- runtime `CCER=0x1555` and `CCR4=3996`.

The stock binary itself is intentionally not committed.

## Sensorless continuity already present

The v0.9.19 lineage retains:

- `sensorless_control.c`;
- the fixed-point VESC Ortega / EBiCS flux observer;
- synthetic PMSM tests for ALIGN -> OPEN -> HANDOVER -> CLOSED;
- ADC -> sector reconstruction -> FOC/SVPWM -> TIM1 pipeline tests;
- v0.9.9 RAM-only atomic Motor-ID adoption into `sensorless_control_t`;
- v0.9.14 VESC-style R/L/Flux measurement sequence;
- v0.9.17 latched terminal MOE stop;
- v0.9.19 read-only ADC proof.

Therefore the remaining job is **not to invent sensorless control again**.
It is to qualify the real G30D electrical interface sufficiently to permit a
bounded first sensorless run.

## Deliberate gate

v0.9.20 keeps normal E5 / sensorless motor drive compile-blocked. The next
torque-producing sensorless image may only be produced after the hardware
evidence closes the current-scaling and PWM-synchronous sampling gates and the
terminal gate-off contract remains intact.

Motor-ID E7 remains a separate bench path. Passing Motor-ID is not by itself a
road-use qualification.

## v0.9.20 CI contract

`integration/tools/test_v120_sensorless_contract.py` is run against the fully
reconstructed v0.9.19 source and fails if later edits accidentally:

- remove the VESC/EBiCS observer;
- introduce Hall dependence into the sensorless controller;
- break Motor-ID -> sensorless-parameter adoption;
- remove the sensorless PMSM / pipeline regression tests;
- enable ordinary sensorless drive before the hardware gate is explicitly
  opened.

This is a continuity/safety checkpoint, not a hardware-validation claim.
