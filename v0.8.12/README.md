# DeltaESC G30D v0.8.12 — ADC offset quality veto (source only)

**GATES OFF / NO FLASH / NOT MOTOR READY.** Built on frozen v0.8.11 fixed injected two-rank STM32F103 ADC1/ADC2 diagnostics; no dynamic JSQR writes inside ADC ISR. The NinebotCrypto/dashboard/PA2 USART2 communication and BLE handlers are unchanged.

## Actual defect found in v0.8.11

An E0 current-sensor offset calibration could complete with 512 samples and `offset_valid=1` **even when the two sequential CH5 injected ranks disagreed by over 128 ADC counts**. That warning was nonfatal and an otherwise "completed" mean could be passed to the observer calculations. Furthermore no range/noise limit was applied across the 512 collected samples. These are not proof of a real zero-current baseline, and certainly cannot authorize motor drive.

v0.8.12 rejects both failure modes:

- Within the E0 512-cycle offset collection, sequential CH5 rank values must differ by **at most 32 raw ADC counts**; exceeding this aborts the entire calibration, with `G30_SCAN_CAL_UNSTABLE` (0x20).
- For each of CH3/CH4/CH5, the **peak-to-peak ADC span** across the 512 samples must not exceed **64 raw ADC counts**. Crossing the limit aborts calibration and invalidates offsets. The collected samples are not averaged into an accepted calibration.
- Calibration remains blocked during IAP, poweroff, throttle/brake input, armed power stage or an existing safety fault; and bad/unqualified offsets are not forwarded to the simulated sensorless FOC regulator.
- After a rejected calibration, the interrupt fault path forces power-stage OFF and sets the safety latch. E0 cannot be repeated until a controlled reset/clear procedure. No newly allowed motor commands are added.
- A 128-count CH5 disagreement **outside** calibration still produces a diagnostic warning. It is not misrepresented as a physically measured zero-current offset.
- The old ADC raw-invalid counter now increments only for truly out-of-range ADC readings; bad JSQR goes into the channel-mapping error counter. Noise errors do not masquerade as a faulty ADC word.

**Both limits (32 and 64 counts) are conservative software diagnostic hypotheses**, not measured hardware current gain, not absolute current zero, and not a hardware safety certification. Their suitability must be verified using real captured voltage/current waveforms.

## New read-only F6 (16-byte little endian)

| Bytes | Field |
|---|---|
| 0..5 | CH3, CH4, CH5 minimum raw ADC counts during current/latest offset calibration, three u16 |
| 6..11 | CH3, CH4, CH5 maximum raw ADC counts during current/latest calibration, three u16 |
| 12..13 | maximum observed discrepancy between sequential CH5 ranks, u16 |
| 14..15 | accepted calibration samples so far (up to 512), u16 |

EF (existing 16 bytes) remains raw values/offsets/duplicate CH5 and status flags; new fatal bit 0x20 reports calibration instability. Build ID `D0 DESC 0x080C`. Current DashBLE ADC v0.3.4 does not unlock this firmware build, and no new parameter-write compatibility has been granted. F6 is a read-only Ninebot packet.

## Software validation

A pinned source overlay at `v0.8.12/overlay.b64.part-*` decompresses to `v0812_offset_overlay.tar.xz`, SHA256 **8af936d98b6aa5d54066d8e2daf45c82506a7ac808ff1d715ed1d7f019775b44**, and reconstructs the complete source from v0.8.11 exactly. Host regression with Clang UBSan tests: stable 512-cycle mean, CH5 duplicate divergence at 33 counts, 65-count sample span, F6 encoding, diagnostic warning outside calibration, invalid JSQR/ADC flags, no output-power arming. GitHub Actions additionally checks Cortex-M3 compilation, negative gate-enable compilation, encrypted 130-block Ninebot UART/IAP error handling and F6 actual ARM READ-ACK.

GitHub Actions exports **only source files**. The local ARM binary is a compile test, **not a distributable firmware image**. No motor, ESC, or stock bootloader/recovery has been physically tested.

## Physical blockers and next work

1. Verify physical ADC1+ADC2 sampling, low-side switching window and simultaneous rank timing on a **recoverable bench controller** with power stage disabled.
2. Validate actual shunt gain/polarity, phases, 10S bus scaling and that the calibration test corresponds to true zero current. Do not assume 14S.
3. Independently demonstrate a successful backup/stock rollback path before modifying the valuable original DRV126. ST-Link is reserved for recovery; usual diagnostics through phone + original NinebotCrypto dashboard BLE.
4. Only after these can a separate, intentionally active motor firmware be developed and bench-tested with validated gate deadtime, OC/break handling and no unexpected braking.

**Keep the original scooter on stock firmware.**
