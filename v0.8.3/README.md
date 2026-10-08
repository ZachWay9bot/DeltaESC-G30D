# DeltaESC G30D v0.8.3 — stock dual-ADC acquisition plan

**SOURCE-ONLY. GATES COMPILE-DISABLED. NOT A FLASH OR ROAD RELEASE.**

v0.8.3 continues from the CI-green v0.8.2 stock-current frontend. It adds the register-level acquisition plan needed to reproduce the original DRV126 current-sampling topology without yet replacing the live ISR.

## Recovered DRV126 runtime topology

- ADC1 = master, ADC2 = slave.
- ADC dual mode = injected simultaneous (`ADC1->CR1 DUALMOD = 0x1`, value `0x00010000`).
- Both ADCs use one injected rank for runtime phase current.
- TIM1_CC4 triggers the master injected conversion.
- ADC1 JEOC drives the current-control interrupt.
- ADC1 JDR1 + ADC2 JDR1 are read as the simultaneous pair.
- The pair for the following PWM period is selected after the current-control pass.

Sector pair map is inherited from v0.8.2:
1/6 -> CH4 + CH5, 2/3 -> CH3 + CH5, 4/5 -> CH3 + CH4.

## What v0.8.3 adds

- `stock_dual_adc_plan.c/.h`: deterministic master/slave register plan.
- Explicit ADC2 register base and RCC clock requirement represented in the plan.
- Exact single-rank JSQR values and master trigger configuration.
- A transition function for the post-JEOC sector pair update.
- Host regression tests for init, all six sectors and trigger/interrupt ownership.
- Build identity 0x0803.
- Hardware commissioning blockers remain unchanged and asserted.

## Deliberate boundary

The existing live ADC ISR is not switched yet. The current E0 offset calibration still depends on sequential CH3/CH4/CH5 sampling, and the existing fourth injected sample is also used as raw VBUS telemetry. Those two responsibilities must be split cleanly before dual-ADC runtime is activated.

So v0.8.3 is the register-accurate acquisition layer, not the live switchover. No new SHU ZIP or flashable firmware is exported.
