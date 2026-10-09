# DeltaESC G30D v0.8.10 — STM32 ADC dual-mode correction

**SOURCE ONLY. SIX GATE OUTPUTS COMPILE-DISABLED. NOT A FLASH OR RIDE RELEASE.**

This version fixes a real STM32F103 hardware-register error in v0.8.9.
The original source had ADC1_CR1.DUALMOD[3:0] = 0001 (0x00010000),
which selects combined regular simultaneous + injected simultaneous mode.
The motor firmware was independently triggering ADC1 regular channel PA1
to measure VBUS, and wanted only the injected ADC1/ADC2 current pairs
to be simultaneous.

RM0008 defines 0101 (0x00050000) as injected simultaneous mode ONLY.
This fix changes the source plan and actual ADC initialization to 0101,
preserving ADC1-only regular VBUS conversions. The change is verified
against ST RM0008 and STMicroelectronics STM32F1 HAL ADCEx definitions.

Official documentation:
https://www.st.com/resource/en/reference_manual/cd00171190-stm32f101xx-stm32f102xx-stm32f103xx-stm32f105xx-and-stm32f107xx-advanced-arm-based-32-bit-mcus-stmicroelectronics.pdf
https://github.com/STMicroelectronics/stm32f1xx-hal-driver/blob/master/Inc/stm32f1xx_hal_adc_ex.h

## Fail closed when ADC2 is not finished

At ADC1 injected end-of-conversion interrupt entry, the firmware now
checks that ADC1 dual mode is 0101, ADC2 reserved mode bits remain zero,
and ADC2 has JEOC. Previously it read ADC2_JDR1 without confirming ADC2
finished, potentially treating a stale phase-current sample as current.

With a fault it does not read the stale ADC2 JDR1; it uses invalid raw
0xFFFF, marks the ED diagnostic record invalid, disarms the motor,
invalidates current-offset calibration, and skips FOC current processing
for this pair. Latched safety fault 0xA007 = ADC2 JEOC absent,
0xA008 = incorrect dual-ADC mode; older fault codes remain unchanged.

**Critical qualification note:** The expected ADC2 JEOC behavior and
frequent sector-by-sector JSQR changes in multimode must be confirmed
against the real STM32F103. RM0008 warns that channel reconfiguration in
dual ADC mode can restart and desynchronize conversions. Never defeat
the conservative fault guard just to make a test pass.

## New read-only EE page, 16-byte little endian

| Bytes | Contents |
|---|---|
| 0..3 | Captured ADC1_CR1, uint32 |
| 4..7 | Captured ADC2_CR1, uint32 |
| 8..11 | Captured ADC1_CR2, uint32 |
| 12..13 | Saturating ADC operating-mode / completion fault count |
| 14 | Last fault mask: 1 wrong ADC1 mode; 2 ADC2 reserved mode; 4 ADC2 JEOC absent |
| 15 | Any mode/completion fault ever latched |

Previous E8 to ED pages and phone BLE protocol are unchanged.
D0 version is 0x080A (hex A is minor release 10). DashBLE v0.3.4
currently supports E8 only on 0x0805/0x0806; it must not be silently
unlocked for v0.8.10. No new phone write permissions are added.

## Tests and hardware limitation

CI reconstructs the v0.8.9 safe source, applies the guarded v0.8.10
patch exactly once, builds the Cortex-M3 firmware with gate outputs
compile-disabled, rejects any attempted active-motor build, runs
host UBSan tests and full NinebotCrypto/UART/IAP emulation including
the new 16-byte read-only EE response. The downloadable artifact
contains **source code only**.

The actual G30D controller has not run this firmware. Gate-driver
pin mapping/polarity, current-shunt gain and direction, ADC2 JEOC on
real hardware, actual ADC trigger phase, reliable stock rollback and
motor R/L/Flux still require hardware validation on a recoverable
test ESC. 10S first; 14S not qualified. No flashing on the valuable
original controller. ST-Link remains recovery only; ordinary diagnosis
uses the stock dashboard and phone NinebotCrypto over BLE.
