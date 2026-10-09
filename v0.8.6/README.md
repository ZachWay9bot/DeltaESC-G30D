# DeltaESC G30D v0.8.6 — ADC integrity + consistent DashBLE telemetry

**SOURCE-ONLY / GATE-OFF / HARDWARE-UNVALIDATED. Do not flash this as a motor or recovery release.**

v0.8.6 builds on frozen CI-green v0.8.5 and keeps the stock-dashboard
NinebotCrypto/PA2 USART2 BLE transport, dashboard power button/hold, staged
update logic, v0.8.5 ADC1/ADC2 injected-simultaneous current acquisition,
F0–F5 atomic RAM parameter transaction and read-only E8 layout unchanged.

## Fixes

- **Atomic E8 snapshot.** BLE reads now block ADC interrupts only while
  encoding the 16-byte E8 payload, so channel/sector/raw samples cannot be
  pulled from different ADC interrupt cycles. Low three E8 status bits and
  all existing fields remain compatible with DashBLE ADC v0.3.3.
- **ADC integrity validation before the motor model.** Every injected
  ADC1/ADC2 sample is checked for 12-bit range (0..4095) and for the
  expected injected ADC channel pair corresponding to sectors 1..6.
  Invalid data is not passed into per-phase offset calibration or FOC.
  A fault is latched (`0xA004`), power stage forcibly disarmed and
  offset calibration invalidated. The acquisition routine advances to
  the next passive channel pair for diagnostics.
- **Fast ADC-stall watchdog.** The main loop notices absence of fresh ADC
  interrupts after 5ms rather than only once each second and latches
  `0xA005`. Detection is active only after at least one ADC sample.
- **Passive timing quality counters.** Samples more than 2× nominal
  16kHz period apart or less than half the nominal period apart are
  counted as anomalies, not used to authorize or block drive. True
  hardware timing is not measured yet, and our provisional thresholds
  must be confirmed on the real controller.
- **E9 read-only** 16-byte status extension. No write commands or new
  phone features required to preserve the working BLE link.

## E9 little-endian data layout

| Byte | Field |
|---|---|
| 0..3 | total checked injected ADC sample pairs, u32 |
| 4..5 | invalid raw (>4095) count, saturating u16 |
| 6..7 | wrong sector/channel-register mapping count, saturating u16 |
| 8..9 | long inter-sample gap count, saturating u16 |
| 10..11 | unexpectedly short inter-sample interval count, saturating u16 |
| 12 | last sample quality bitfield: 0x01 bad range; 0x02 wrong mapping; 0x04 long gap; 0x08 short interval |
| 13 | any invalid range/mapping latched: 0 or 1 |
| 14..15 | low 16 bits of latched safety fault |

The E8 status byte at offset 11 also exposes **new non-breaking bits**:
0x08 integrity failure latched, 0x10 long gap in most recent sample,
0x20 short interval in most recent sample. The older 0x01/0x02/0x04
offset-calibration/VBUS flags are retained. DashBLE v0.3.3 will still
decode E8; its UI may not yet label the additional flags.

**Caveat on raw samples:** An exact E8 snapshot prevents software-level
tearing, but does not prove ADC1 and ADC2 physically sampled
simultaneously. That needs measurements or verified dual-ADC status
on the user's physical controller.

## Tests and compatibility

- 64MHz STM32F103 safe-target ARM build using `-DPOWER_STAGE_ARM_ALLOWED=0`
  and `-DSENSORLESS_RUN_ALLOWED=0`, with hard compile-time refusal
  when either is enabled.
- Host UndefinedBehaviorSanitizer test for all six sectors, corrupt
  12-bit samples, bad ADC JSQR channel, timing anomalies, null
  state, saturating counters; retain existing 512-sample-per-channel
  dual-ADC calibration test.
- Regress complete Ninebot BLE UART simulator, staged IAP error
  branches, dashboard power/hold, motor math, E8 and new E9 read-ACKs.

The **human-tested** Android connection remains DashBLE ADC v0.3.3.
Its configuration write guard remains **exact firmware 0x0804**;
v0.8.6 introduces no new untested parameter write support.
The physical ESC has so far identified as stock 0x0907, not DeltaESC.

**ST-Link is for recovery only. Phone over the stock Ninebot dashboard
is the config/diagnostic path.** Current sensing polarity/gain, ADC
sample-window timing, TIM1 six gate polarities/deadtime, BMS behavior,
voltage divider, motor R/L/flux and stock firmware rollback remain
hardware blockers. This is not a ride-ready firmware.
