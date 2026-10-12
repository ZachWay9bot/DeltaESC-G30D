# DeltaESC G30D v0.9.20 – sensorless locked integration

**SOURCE/LINK VALIDATION ONLY · NO FLASH · NO MOTOR AUTHORIZATION**

The project target remains a **Hall-free sensorless FOC controller** for the
Ninebot G30D Gen1 STM32F103. v0.9.20 does not retreat to Hall control.

This step combines the current v0.9.19 chain with the complete VESC/EBiCS-style
sensorless software path while deliberately keeping physical bridge arming
compile-locked.

## What this proves

- the VESC/EBiCS fixed-point flux observer still builds for Cortex-M3;
- the sensorless `STOP -> ALIGN -> OPEN -> HANDOVER -> CLOSED` state machine is
  linked into the same source tree as the DRV126 ADC/current reconstruction;
- the v0.9.16 BDTR guards and v0.9.17 terminal MOE kill remain present;
- v0.9.18/v0.9.19 read-only FA/FB ADC evidence remains present;
- the sensorless build uses `SENSORLESS_RUN_ALLOWED=1` and
  `SENSORLESS_CLOSED_LOOP_ALLOWED=1`, but **POWER_STAGE_ARM_ALLOWED=0**;
- the v0.9.20 target intentionally produces an ELF/map/section report, not a
  convenient flashable BIN.

## What this does not prove

It does not prove MT8006A electrical isolation, current gain in amperes, real
ADC sample timing under switching, AMP_LIM behaviour, power-latch routing,
motor parameters or successful observer handover on the physical wheel.

A successful CI build is therefore not a flash approval.

## DRV126 full-dump evidence

The supplied 128 KiB stock DRV126 full dump was inspected with
`tools/audit_drv126_fulldump.py`.

Observed on the supplied image:

- size: **131072 bytes**
- SHA-256: `9235b466a2f7449b8184560dbef9012d7c12b099e4076100a223ef68d204bb68`
- stock boot vector: SP `0x20000550`, reset `0x08000121`
- application vector at `0x08001000`: SP `0x20002B18`, reset `0x08001101`
- literal references found for TIM1, ADC1, ADC2, GPIOA/B/C, RCC, DMA1 and
  USART1/2/3, consistent with those peripherals being used by stock firmware.

Those are **static binary facts**, not proof of PCB routing or analog/gate
electrical behaviour. The dump itself is not committed to this repository.
