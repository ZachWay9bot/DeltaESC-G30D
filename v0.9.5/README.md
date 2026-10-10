# DeltaESC G30D v0.9.5 – Stock DRV126 TIM1 Gate-Backend

**Scope: ONLY GATE path (Hall and sensorless independent).**

The known stock DRV126 SVPWM function `0x08005510` writes `TIM1_CCER = 0x1555` around `0x0800571A`. This enables TIM1 CH1/1N, CH2/2N, CH3/3N plus CC4 ADC trigger. v0.9.5 uses the same *full register value*, rather than retaining stale phase/CC4 polarity bits from an OR operation. On STOP, BRAKE, throttle release, observer fault or invalid ADC sample, the existing common `motor_pipeline_stop` calls the gate-off interface before any software neutral vector. That interface clears all six motor CCER enables but retains the CH4 sampling trigger. PB1 is not driven as a gate-enable.

The GPIO alternate-function route uses PA8/9/10 and PB13/14/15. No assumption on Hall sensor state affects this code. The new pure gate register module `stock_tim1_gate.[ch]` is directly wired to the real Cortex-M3 main motor arming, stop and periodic safety-check paths. The surrounding v0.9.4 ADC→FOC→SVPWM→TIM1 integration is retained unchanged.

**Tests:** `bash tools/run_v095_tests.sh` passed on Cortex-M3 link with 18,428-byte SYNC_SAFE binary, 17 earlier host test groups and 1 new gate driver test spanning every 16-bit CCER input (65,536 values). Both deliberate powered-build attempts remain denied because `COMM_CURRENT_SCALE_HW_VALID` and `COMM_ADC_TIMING_HW_VALID` are still false; this is *not* another gate-pinout blocker. `COMM_GATE_STOCK_PROFILE_READY=1` means *stock gate register route is implemented*, not that custom MOSFET/overcurrent behavior has been demonstrated.

**STATUS: SOURCE + SIMULATED REGRESSION ONLY.** Installed SHFW unchanged. Do NOT flash this development variant to a scooter or use for motor operation. The next separate task is safe phase-current/sampling acceptance; the gate driver implementation itself is complete against the DRV126 register evidence.