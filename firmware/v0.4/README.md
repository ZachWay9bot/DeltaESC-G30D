# DeltaESC G30D clean v0.4 — PWM + ADC synchronization bring-up

This continues the clean sensorless STM32F103 line. It is **not** the older SmartESC/DeltaESC 48 kB branch and it contains no SHU packaging.

## What v0.4 proves

v0.4 moves one layer closer to real motor drive without yet commanding torque:

- CPU remains at 64 MHz so timing stays comparable with v0.3.
- TIM1 runs center-aligned at 16 kHz, ARR=1999.
- CH1/1N, CH2/2N and CH3/3N are the future three bridge half-bridges.
- Dead-time generator is DTG=64, approximately 1 us at the current timer clock.
- TIM1 CH4 is used only as an internal ADC trigger. PA11 stays GPIO HIGH as the G30 power-hold output and is never configured as TIM1_CH4 alternate function.
- ADC1 injected sequence is hardware-triggered from TIM1_CH4 and samples PA3/PA4/PA5 phase-current channels plus PA1 bus sense.
- ADC runs once per 16 kHz PWM period. The observer/current-control timing path runs every fourth sample = 4 kHz.
- DWT records ADC interrupt spacing, ISR time and 4 kHz control-core time.

STM32F103 RM0008 defines TIM1_CC4 as a valid injected ADC trigger for ADC1/ADC2. The G30 pin mapping used here is PA8/9/10 + PB13/14/15 for TIM1 bridge outputs, PA3/4/5 for phase-current sensing, PB1 for gate-driver enable and PA11 for power hold.

## Two binaries

### `DeltaESC_G30D_v0_4_pwm_syncsafe.bin`

This is the first binary to flash.

- power-stage arming is removed at compile time;
- PB1 remains LOW;
- all six bridge pins remain GPIO inputs with pull-downs;
- TIM1 CH4 + MOE are active only so the internal ADC trigger exists;
- CH1/2/3 gate-channel enable bits remain clear;
- no physical bridge PWM can be produced.

### `DeltaESC_G30D_v0_4_zero_vector_active.bin`

This is only for the scope/bench step after the sync-safe build passes.

It still boots DISARMED. UART1 on PB6/PB7, 115200 8N1, accepts:

- `A` — arm the bridge only if all preconditions pass;
- `D` — immediate software disarm;
- `C` — clear timing/current peak statistics;
- `?` — print help/status.

When armed, v0.4 outputs equal 50% duty on all three phases. That produces nominally zero line-to-line voltage and therefore no intended motor torque. The purpose is to verify complementary PWM, polarity, gate-driver enable and dead-time with an oscilloscope before any rotating-vector command is permitted.

The firmware will refuse ARM until:

- at least 3 seconds have elapsed;
- at least 2000 synchronized ADC samples have arrived;
- ADC triggering is not stale;
- phase-current residual is below 120 ADC counts;
- bus ADC is in a sane non-rail range;
- no safety latch is active.

Once armed, an instantaneous phase-current residual above 700 ADC counts forces PB1 LOW and removes all six TIM1 bridge channel enables. This is a raw-count emergency guard, not the final calibrated ampere current limit.

## Sensorless control status

The fixed-point sensorless observer/current-control/SVPWM path still runs for timing and data-path validation, but **its calculated CCR values are not applied to the bridge in v0.4**. R/L/flux and current scaling are still placeholders.

The next stage after v0.4 hardware validation is the first torque-producing sensorless bring-up:

1. calibrate real current scaling and signs;
2. verify phase mapping with low-energy test vectors;
3. measure/enter motor R and L;
4. implement limited open-loop electrical-angle start;
5. converge and blend into the observer;
6. only then enable closed-loop sensorless FOC torque.

No Hall input is required by this path.

## Flash layout

- application base: `0x08001000`
- executable ceiling: `0x0800D800` (50 KiB window)
- full 128 KiB backup remains the recovery source
- SHU/OTA support intentionally deferred

These `.bin` files are relocated application images, not full-flash images.

## Canonical build

The canonical CI toolchain is pinned to **Ubuntu 24.04 + clang/LLVM 18.1.3**. GitHub Actions builds both binaries, runs the static preflight, prints SHA-256 hashes and uploads the binaries as the `DeltaESC-G30D-v0.4-bench` artifact.

Canonical v0.4 CI output:

- sync-safe: 4328 bytes, SHA-256 `856763bd4e3da477404365ca1fdf097712292be6aea3d431280bbaed2954831c`
- zero-vector active: 4648 bytes, SHA-256 `66e13c2c18709f4277c95c00b716cfc25fc351673627ca0c115b8e4f34634767`

Build locally with LLVM 18:

```sh
cd firmware
make clean all CC=clang-18 OBJCOPY=llvm-objcopy-18 OBJDUMP=llvm-objdump-18
python3 tools/preflight.py
```

The current source tree intentionally does **not** store flashable v0.4 binaries directly in Git. Use a hash-verified CI artifact or rebuild with the pinned toolchain. This prevents accidental publication of a truncated or otherwise altered firmware image.
