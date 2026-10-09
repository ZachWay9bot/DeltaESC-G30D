# DeltaESC G30D v0.8.8 — TIM1 / ADC injected-IRQ timing evidence

**SOURCE-ONLY, GATES COMPILE-DISABLED, NOT TO BE FLASHED ON THE ORIGINAL ESC.**

v0.8.8 adds read-only TIM1 hardware-register evidence to frozen v0.8.7.
It retains v0.8.7's synchronized ADC1/ADC2 raw+JSQR records E8/E9/EA,
v0.8.6 fail-closed 12-bit ADC validity and sector/channel checks,
the F0-F5 atomic RAM configuration, staged-IAP error guards, dashboard
power button and the phone-proven NinebotCrypto/PA2 USART2 transport.

## The crucial timing distinction

**These timestamps represent entry into the ADC1_2 interrupt service routine, not the actual analog sample instant.**

The ADC converter must finish and the CPU must enter the handler
before we read TIM1_CNT or DWT_CYCCNT. ISR latency and conversion
time are included; neither is hardware-calibrated. Center-aligned TIM1
also has distinct rising/falling count directions, exposed through
TIM1_CR1. It would be unsafe to treat a single TIM1_CNT value as the
real physical low-side shunt sampling location.

## New read-only 16-byte registers, little-endian

### EB: timer hardware snapshot (taken at ADC ISR entry)

| Offset | Contents |
|---|---|
| 0-1 | TIM1_CNT u16 at ISR entry |
| 2-3 | TIM1_CCR4 u16 trigger compare configuration |
| 4-5 | TIM1_ARR u16 period register |
| 6-7 | TIM1_CR1 u16 (CEN/CMS/DIR/ARPE) |
| 8-9 | TIM1_CCER u16 (motor complementary channels must remain disabled) |
| 10-11 | TIM1_BDTR u16 |
| 12-15 | DWT_CYCCNT u32 at ADC ISR entry |

### EC: timer diagnostic events

| Offset | Contents |
|---|---|
| 0-3 | ADC ISR sample sequence number u32 |
| 4-7 | difference in DWT IRQ-entry cycle counters, u32; first sample is zero |
| 8-9 | provisional timer configuration flags u16 |
| 10-11 | saturating u16 count of configuration anomalies |
| 12-13 | TIM1_SR u16 at ADC ISR entry |
| 14-15 | reserved u16 (zero) |

Flags: bit0 timer not enabled, bit1 not in center-aligned mode,
bit2 CC4 output not enabled, bit3 any TIM1 motor gate channel enabled,
bit4 invalid ARR/CCR4 compare, bit5 TIM1 MOE not set. The monitor
**does not authorize** any PWM even if all flags are clear.

Existing D0 build is now 0x0808. E8/E9/EA semantics are unchanged.
For any interpretation of EB/EC in a future DashBLE client, require
NinebotCrypto, DESC signature and an **exact 0x0808 build**. Existing
DashBLE ADC v0.3.4 supports E8 only on 0x0805/0x0806; it does NOT
claim live diagnostic compatibility with build 0x0808.

## Software CI

- Full STM32F103 Cortex-M3 clang safe-only application build, and
  negative compilation attempt which must reject motor gate enabling.
- UBSan host regression for TIM1 CEN/CMS/CC4E/CCER/MOE and compare
  bounds, DWT cycle-counter overflow, EB/EC byte encoding, counter
  saturation, plus the pre-existing six-sector dual ADC/FOC host tests.
- Full ARM/Unicorn NinebotCrypto UART/IAP and error-path regressions;
  exact read-ACK of E8, E9, EA, EB and EC, each 16 bytes.
- Export of complete source only, no firmware .bin/SHU flashing zip.

## What the physical ESC still needs

ADC/TIM1 capture code passing on an emulator does not verify TIM1's
actual PWM timing, ADC1/ADC2 simultaneous conversion, shunt amplifier
gain/polarity, GPIO routing, complementary-gate deadtime/polarity or
the safe recovery path after an IAP erase. Those require controlled
hardware testing on a spare/recoverable controller with the bridge
disconnected or otherwise physically disabled.

Before real motor torque, also measure R/L/flux on the actual wheel
and qualify throttle-release coast, brake motor-off, overspeed,
overcurrent, watchdog and 10s supply behavior. 14s operation remains
unsupported pending hardware divider validation.

**Do not flash to the valuable original G30D ESC.**
ST-Link is reserved for recovery; phone/DashBLE over stock dashboard
BLE remains configuration and diagnostics.
