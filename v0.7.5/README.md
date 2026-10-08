# DeltaESC G30D v0.7.5 provisional G30 motor profile

Status: **bench-only development**. Current limits remain 100..500 mA. No road use.

Public G30 Motor Control Workbench data provides:
- Rs = 0.17 ohm per phase
- Ls = 0.000312 H per phase
- 15 pole pairs
- Ke = 40 Vrms phase-phase / krpm
- PWM = 16 kHz

v0.7.5 changes only the default R/L profile to the published G30 values:
- R = 170000 uohm
- L = 312000 nH

Flux remains at the previous provisional 1800 internal units and stays configurable through F2. It is deliberately NOT derived automatically from Ke until the fixed-point observer scaling is validated against hardware telemetry.

All v0.7.2-v0.7.4 safety behavior remains unchanged:
- PB12 TIM1 BKIN
- PA3/PA4/PA5 U/V/W current mapping
- offset-minus-ADC current polarity
- phase-sum and peak-current trips
- 100..500 mA E5 limit
- Q15 PI accumulator
- widened/filtered observer speed and guarded forward handover

Build ID: 0x0750.
