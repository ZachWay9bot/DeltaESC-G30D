# DeltaESC G30D v0.8.7 — correlated ADC1/ADC2 evidence

**SOURCE ONLY, MOTOR GATES OFF, NOT FLASH-READY OR RIDE-READY.**

Frozen base: v0.8.6, including phone/dashboard/PA2 NinebotCrypto,
E8 ADC raw values, E9 integrity counters, the parameterized sensorless
regulator and the hardware commissioning guard.

## Real issue fixed

In v0.8.5/v0.8.6 the injected ADC ISR collected the raw ADC1+ADC2
measurements for sector N and then immediately programmed JSQR channel
selection registers for sector N+1. E8 reported the raw values and the
completed sector N, but its last 16-bit field came from the live ADC2_JSQR
register (already sector N+1). The interrupt-masked E8 packet assembly
prevented concurrent tearing but did not correct that historical mismatch.

v0.8.7 records raw JDR1, full JSQR and ADC status from **both converters
inside the same completion interrupt**, before changing sectors. The
ADC-quality checker now evaluates the saved JSQR values for the completed
pair, and E8's legacy 16-bit JSQR field comes from that same saved pair.

### New read-only EA (0xEA), 16 bytes, little-endian

| Bytes | Field |
|---|---|
| 0–3 | last completed ADC1 JSQR, full u32 |
| 4–7 | last completed ADC2 JSQR, full u32 |
| 8–9 | ADC1 SR captured at IRQ entry, u16 |
| 10–11 | ADC2 SR captured at IRQ entry, u16 |
| 12–13 | ADC1 injected JDR1 raw, u16 |
| 14–15 | ADC2 injected JDR1 raw, u16 |

The matching sector is exposed in E8 byte 8. The upper JSQR bits in EA
contain the actual selected channel, unlike E8's legacy low-16 fragment.
This is register evidence, not a calibrated amperage measurement.

STM32 RM0008 describes injected simultaneous ADC1 master triggering both
converters and combined conversion completion; we do not introduce an
unverified busy-wait loop for ADC2 JEOC.

## Invariants retained

- Both hardware gate/closed-loop build feature macros remain 0 and
  the target compilation intentionally rejects attempts to enable them.
- Six bridge pins remain inputs; PB1 stays disabled.
- v0.8.6 ADC range/sector/integrity faults and ADC stale detection remain.
- Original NinebotCrypto, dashboard power button, power hold, and
  protected staged update paths are left unchanged.
- F0–F5 RAM parameter transaction unchanged. The phone application's
  existing exact-0x0804 write guard is *not* broadened.
- Existing v0.3.4 Android E8 UI intentionally permits only 0x0805/0x0806
  until a separately tested v0.8.7 decoder is released.

## Validation

GitHub CI rebuilds v0.8.6 from the pinned source, applies the v0.8.7
overlay, cross-compiles a Cortex-M3 safe-only binary, requires a
negative gate-enable build to fail, tests all six ADC sector records
under UBSan, and runs the existing motor numeric, crypto, UART,
IAP error-path and E8/E9/EA ARM simulation regressions.

**A green CI does not validate the physical ESC or its bootloader.**
No original G30D flash or motor operation is authorized.

## Hardware blockers

Physical ADC current gains and signs, trigger timing and PWM-low-side
window, TIM1 complementary gate polarity/deadtime, DRV126 pinout,
10S voltage scale, motor R/L/flux, and stock-rollback reliability
still require physical validation on a recoverable controller.
ST-Link remains for emergency recovery only. Phone/dashboard BLE remains
the normal configuration and diagnostics path.
