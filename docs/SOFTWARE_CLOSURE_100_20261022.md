# DeltaESC software closure — 22 October 2026

This branch closes the requested software acceptance scope without changing production motor control, IAP, bootloader, linker, recovery or current limits.

Software acceptance contract:
- Drive path: start/stop/rearm ownership, brake priority, dashboard timeout, IAP ownership, ADC sequencing, PWM refusal, raw-count overcurrent and speed-domain boundaries.
- FOC/sensorless: existing synthetic PMSM tests cover align/open-loop/handover/closed-loop/observer-loss; rolling BEMF tests cover direction, speed threshold, phase seed and qualification loss.
- Dashboard/watchdog: valid 0x65 control, malformed ownership/length frames, brake/coast/rearm, stale-neutral expiry, 0x64 not refreshing control, powered timeout, status rounding/clamping and uint32 millisecond wraparound.

100% here means 100% of the currently specified software acceptance contract. It does not mean physical G30 validation, calibrated current/VBUS values, measured target motor parameters or physical IAP rollback proof.
