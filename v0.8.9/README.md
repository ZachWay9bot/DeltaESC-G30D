# DeltaESC G30D v0.8.9: Atomic ADC/TIM1 evidence (SOURCE ONLY)

**NOT FLASH-READY, NOT RIDE-READY. SIX GATES COMPILE-DISABLED.**

v0.8.8 exposes synchronized individual ADC sample (E8/EA) and timer
register (EB/EC) pages. These packets are read one after another across
a phone BLE link, during which the 16 kHz ADC ISR keeps running.
An individual page can be coherent while the multi-page set does not
belong to the same interrupt. Do **not** correlate values from
independent pages as if they shared a timestamp.

v0.8.9 introduces one new 16-byte read-only register **ED (0xED)**.
It is populated atomically from the captured ADC1/ADC2 and TIM1
snapshots by the *same completed injected ADC ISR*, before programming
the next pair of ADC channel registers. The snapshot persists unchanged
until a later completed ISR and is copied under interrupt masking for
the Ninebot packet. Other existing pages retain their ABI.

| ED offset | Field |
|---|---|
| 0..3 | injected ADC interrupt sequence, little-endian u32 |
| 4..5 | completed ADC1 JDR1 raw, u16 |
| 6..7 | completed ADC2 JDR1 raw, u16 |
| 8..9 | TIM1 CNT at IRQ entry, u16 |
| 10..11 | TIM1 CCR4 as configured at IRQ entry, u16 |
| 12 | completed sector (1..6) |
| 13 | ADC1 injected channel decoded from captured full JSQR |
| 14 | ADC2 injected channel decoded from captured full JSQR |
| 15 | safety/quality flags |

ED flags: bit0 ADC1 JEOC at entry, bit1 ADC2 JEOC at entry,
bit2 invalid 12-bit raw sample or JSQR/sector mismatch,
bit3 unexpected active motor TIM1 outputs, bit4 anomalous
interrupt interval, bit5 timer configuration anomaly.

An ED sequence of zero means no evidence captured since startup.
An ED record alone is evidence of the **completed ISR**, not proof
of the physical analog sampling instant, correct shunt polarity,
ADC gain, or physically safe motor PWM timing. Missing ADC2 JEOC is
exposed as an observation, not blindly used to energize hardware.

The full STM32F103 firmware is rebuilt from pinned, previously
CI-green source and new source patches (all version anchors checked).
CI runs six-sector ED host tests under UndefinedBehaviorSanitizer,
bad-sample and missing-ADC2 flags, DWT/TIM1 numerical tests,
protected motor parameter commit regressions, 130-block encrypted
Ninebot UART/IAP simulation, E8..ED actual ARM read-ACK, and a
negative gate-enable compilation that **must fail**. Only a source
archive is exported.

Build identification is **D0 DESC 0x0809**. Current phone APK
DashBLE ADC v0.3.4 permits E8 on **0x0805 and 0x0806 only**.
It must not claim this build works merely because the older
BLE pairing was verified on the real G30D. A future app decoder
will require *exact* 0x0809 for ED readout and will continue to lock
F0-F5 writes to 0x0804 unless a separate protocol validation occurs.

### Remaining physical blockers

The original valuable DRV126 ESC has no independently proven safe
backup, recovery, or staged-IAP rollback path. All gate pin polarities,
TIM1/dead-time safety, ADC injected trigger window, shunt gain/sign and
motor R/L/Flux need hardware tests on a spare, recoverable controller.
10S only until bus scaling is verified, 14S not authorized.

**Do not flash this source-derived code to the original scooter.
ST-Link is recovery-only. Phone/dashboard BLE remains diagnostics.**
