# DeltaESC G30D v0.6.6 — PA12 power-button recovery

Status: **CI/recovery candidate only. Do not flash until the current controller is restored to stock and the SAFE build has passed a separate bench.**

This revision fixes the faults exposed by the first real-controller v0.6.5 test.

- PA2 / yellow: dashboard USART2 half-duplex only.
- PA12 / green: physical dashboard power-button input, active-low.
- PA11: ESC power-hold / keep-alive output.
- Continuous ~6 s PA12 LOW requests shutdown; IAP activity inhibits shutdown.
- Ninebot register 0x79 write non-zero ACKs first, then releases PA11 after 150 ms.
- Ninebot writes corrected to 0x02 write, 0x03 write-no-reply, 0x05 ACK.
- Standard version reads return stock-compatible 0x0420; DeltaESC D0 remains DESC + build 0x0606.
- Motor control policy is unchanged: SYNC-SAFE uses POWER_STAGE_ARM_ALLOWED=0 and SENSORLESS_RUN_ALLOWED=0.

The PA12 mapping is based on G30 dashboard wiring (green button line, yellow UART) and stock DRV126 reverse-engineering. The previous PA2/yellow button assumption is removed.
