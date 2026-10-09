# DeltaESC G30D v0.7.6 time-boxed motor bench

Status: **bench-only test candidate**. Rear wheel off the ground, 10S, 100..500 mA only.

This version layers one additional safety mechanism on the CI-green v0.7.5 G30 profile:

- Build ID: `0x0760`.
- Every accepted E5 bench-start arms a **firmware-side 1000 ms deadline**.
- When that deadline expires, the firmware calls the same hard disarm path as E6.
- E6 or any other disarm clears the active bench timer.
- D9 is extended from 8 to 12 bytes. Bytes 0..7 stay unchanged; bytes 8..9 report the timeout in ms and bytes 10..11 the auto-timeout count since boot.
- Timeout comparison is wrap-safe across the 32-bit millisecond counter.

All earlier v0.7.2-v0.7.5 protections remain: PB12 TIM1 BKIN, PA3/4/5 U/V/W mapping, offset-minus-ADC current sign, phase-sum / peak-current trips, Q15 PI accumulation, guarded observer handover, provisional G30 R/L defaults, and the 100..500 mA E5 hard limit.

A green CI result is still not road validation.
