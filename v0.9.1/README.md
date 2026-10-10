# DeltaESC G30D v0.9.1: PA11 / PC14 / PB1 hardware-mapping correction

**SOURCE / SOFTWARE-ONLY DIAGNOSTIC — NOT VERIFIED ON THE G30D ESC — DO NOT FLASH OR RIDE.**

Baseline: frozen v0.9.0 source export from GitHub Actions run 37996878172, artefact 11647163262. This revision **does not enable motor outputs** and does not assert that physical gate topology or BKIN break routing is safe.

## Verified from documented DRV126 disassembly and board schematic comparison

* **PWR_BTN = PC14 (STM32 LQFP48 physical pin 3), active HIGH**. The previous PA12 active-LOW button assumption was incompatible with the cited schematic/stock path. Cross-check reported stock PC14 init/reads: `0x080052B8`, `0x0800411C`, `0x080042DC`.
* **PWR_EN / power hold = PA11**. Reported stock power latch set/clear references `0x080052EE` / `0x080051D8`; previous bootloop/brick remains electrically unproven, so no claims that this software fix alone solves it.
* **PB1 = BEMF_C**, **not a confirmed external gate enable**. Eliminated every PB1 direction-change and PB1 BSRR/BRR write from `src/main.c`, including reset/disarm and the compile-disabled arming stub. PB1 must not be driven as a safety output.
* **TIM1 outputs PA8/PA9/PA10 and PB13/PB14/PB15** are plausible alternate-function mappings, but actual high/low MOSFET gate polarity and safe inactive levels still require scope/trace confirmation.
* **PB12 = AMP_LIM** is a possible TIM1_BKIN input on the STM32. Physical routing, polarity, fault-clearing and passive behavior are **not** proven; **BKE remains disabled**. Do not claim hardware OC protection.
* **PA3/PA4/PA5** are assumed analog phase-current inputs; actual shunt transfer function and PWM-coherent sampling windows remain unqualified.

## Code corrections

* `power_hold_init()` preloads PA11 high **before** switching it into a push-pull output, avoiding an obvious transient low-latch on mode change. PA11 physical latch semantics still need scope evidence.
* PC14 is left as a high-impedance input. Long press uses **active-high** sampling on PC14, not PA12; no GPIO output is ever driven on PC14.
* PB1 is no longer configured as an output, read as a gate status, or switched as a disarm action. Safety logic latches a fault if software ever reports armed unexpectedly, rather than relying on an imagined PB1 enable.
* Existing TIM1 six gate-output enables remain disabled at compile time. ADC CH4 trigger is left unchanged for the separate passive diagnostics.
* `POWER_STAGE_ARM_ALLOWED=1` and `SENSORLESS_RUN_ALLOWED=1` are hard compiler errors.

## Remaining blockers

Before another *physical* development flash: confirm installed MCU package, power latch behavior at reset/hold/shutdown, PC14 pull state and button polarity, PB1 BEMF trace, gate-driver device/polarity, separate overcurrent/BKIN path, 10S current-sense polarity and timing, and ST-Link read/write/recovery on a noncritical board. Read-only SWD captures can inform this work but do not independently prove wiring.

**Nothing here is a motor-release or power-stage approval.** It corrects demonstrably dangerous *software assumptions*, retains gate-OFF safety and adds testable regression guards.