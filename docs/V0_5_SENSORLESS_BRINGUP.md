# DeltaESC-G30D v0.5 sensorless bring-up

> **UNVALIDATED HARDWARE TEST BUILD**
>
> v0.5 is a bench-development branch for the stock Ninebot G30D Gen1 3-cap
> STM32F103 controller. Keep ST-Link/SWD recovery connected and use the first
> motor tests on a 10S source with the wheel unloaded. SHU/OTA remains out of scope
> until motor control is proven.

## What changed from v0.4

v0.4 proved the intended hardware skeleton: TIM1 complementary PWM, about 1 us
deadtime, PWM-synchronous injected ADC sampling of IA/IB/IC/VBUS and a 4 kHz
control budget.

v0.5 adds a real sensorless control path:

1. Clarke transform from the three G30 current channels.
2. Fixed-point nonlinear flux observer adapted from the EBiCS \`Sensorless_VESC\`
   implementation, which itself is based on the VESC/MXLEMMING observer family.
3. Observer electrical angle from the estimated flux vector.
4. d/q current control and inverse Park transform.
5. Centered three-phase SVPWM duty generation.
6. A guarded startup path:
   \`STOP -> ALIGN -> OPEN -> HANDOVER -> CLOSED\`.
7. Observer-lock validation before takeover. Loss of a valid observer causes
   \`FAULT\` and returns the bridge to equal-duty zero vector.

No Hall signal is used by the control algorithm.

## Build matrix

### \`DeltaESC_G30D_v0_5_sensorless_syncsafe.bin\`

Power-stage arming is compiled out. Use for timing, UART and ADC verification.

### \`DeltaESC_G30D_v0_5_observer_zero_vector.bin\`

The bridge can be armed manually with \`A\`, but torque-producing sensorless run
is compiled out. This is the next oscilloscope/current-offset validation image.

### \`DeltaESC_G30D_v0_5_sensorless_bench.bin\`

Contains the low-energy ALIGN/OPEN/HANDOVER/CLOSED sequence. It boots disarmed.
\`A\` is required before \`R\` is accepted. \`S\` returns to zero vector and \`D\`
disarms the gate driver.

This binary is a test candidate, not a validated release.

## UART commands

- \`?\` status and help
- \`A\` arm the bridge at equal-duty zero vector
- \`R\` request the sensorless startup sequence (bench image only)
- \`S\` stop motor control while retaining zero vector
- \`D\` disarm the gate driver
- \`C\` clear timing/current statistics

Status includes control state, observer-valid flag, control/observer electrical
angle, phase error, iq/id, observer flux magnitude squared, fault code and raw
ADC channels.

## G30 hardware assumptions used by this branch

The PWM/current layout follows the known G30 Gen1 STM32F103 mapping:

- PA8/PA9/PA10: TIM1 CH1/CH2/CH3 high-side PWM
- PB13/PB14/PB15: TIM1 CH1N/CH2N/CH3N low-side PWM
- PA3/PA4/PA5: three phase-current amplifier outputs
- PA1: current v0.4/v0.5 bus-voltage input assumption
- PB1: gate-driver enable
- PA11: controller power hold
- TIM1 CH4: internal ADC trigger only

The old SmartESC G30 Workbench data lists a 2 mOhm shunt and gain 8. At 3.3 V,
12-bit ADC this corresponds to about 50.35 mA per ADC count. v0.5 uses that only
as the observer current scaling reference.

## Parameters that are NOT hardware-validated yet

The following values remain bring-up/tuning values and must not be treated as
measured G30 motor constants:

- observer resistance parameter
- observer inductance parameter
- flux linkage
- observer electrical phase offset
- exact PA1 bus-voltage calibration
- phase-current sign/order
- final current-loop PI gains

The defaults live at the top of \`src/sensorless_control.c\` and can be overridden
at compile time.

## First hardware progression

1. Flash \`sensorless_syncsafe\`; verify ADC sample timing and offsets.
2. Flash \`observer_zero_vector\`; arm only after checking all six PWM outputs,
   complementary polarity and deadtime on a scope.
3. Confirm IA/IB/IC signs by manually rotating / applying a known low-energy
   vector before permitting handover.
4. Measure or identify motor R/L/flux and set observer constants.
5. Only then use \`sensorless_bench\`: unloaded wheel, 10S, low current, \`A\`, then
   \`R\`.
6. The first acceptable result is not merely rotation. The observer phase must
   track the open-loop angle and HANDOVER must complete without current spikes.
7. Throttle, brake, BMS, SHU packaging, 14S scaling and star/delta remain later
   layers.

## References

- EBiCS/EBiCS_Firmware, branch \`Sensorless_VESC\`
- EBiCS/EBiCS_motor_FOC
- Koxx3/SmartESC_STM32_v3
- Koxx3/SmartESC_STM32_v2 G30 hardware target
- VESC open-source FOC observer implementation
