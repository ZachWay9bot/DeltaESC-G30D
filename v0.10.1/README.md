# DeltaESC G30D v0.10.1 – Rolling Active RC

First **flashable** DeltaESC rolling-sensorless FOC bring-up image derived from
v0.10.0 + XBOT FO69/FO70 evidence.

This is a motor-bench RC, not the final riding release.

## Adopted FO70/FO69 profile

- R phase: 0.583 ohm
- Ld: 0.640 mH
- PM flux linkage: 12.1 mWb
- 15 pole pairs
- 243 mm wheel model / 763 mm circumference
- logical forward BEMF rank step: 5
- rolling start threshold: 3.0 km/h
- Flux -> FOC frame offset: +117.15 electrical degrees (21327 u16 counts)
- rolling closed-loop torque target remains capped to 12 current ADC counts
- first-RC hard over-current trip reduced from 700 to 96 raw reconstructed counts

The existing production qualification flags remain false. v0.10.1 does **not**
rewrite them as calibrated facts. Instead the explicit RC target may arm only
through its dedicated compile capability and still requires the existing boot
offsets, ADC2 CH4/CH5 offsets, same-tick sector/sample-window validation,
dashboard freshness, brake-off state, forward rolling evidence, and the
v0.9.17 terminal MOE/CCER kill path.

## Start path

    DISARMED
      -> dashboard neutral observed
      -> forward roll >= 3 km/h
      -> coherent passive BEMF
      -> E5 with throttle
      -> cancel regular BEMF ADC ownership
      -> seed physical PM-flux observer
      -> ROLL_VERIFY at Iq=0
      -> FO69 phase/magnitude/clamp confidence
      -> bounded Iq ramp
      -> CLOSED sensorless FOC

There is no ALIGN/OPEN fallback in this target.

## Important frame correction

The passive BEMF detector produces a control-frame rotor phase. The nonlinear
observer stores its flux vector in physical alpha/beta coordinates. Therefore
the learned +117.15 degree Flux->FOC offset is subtracted before seeding the
internal observer vector and added again only when the observer angle is mapped
back into the FOC frame. This prevents double application of the calibration.

## Build target

    make rolling-active-rc CC=clang-18 OBJCOPY=llvm-objcopy-18 OBJDUMP=llvm-objdump-18

Output:

    build/DeltaESC_G30D_v0_10_1_rolling_active_rc.bin

The image is intended for the already-used current-limited, wheel-free bench
setup. Recovery remains by ST-Link.
