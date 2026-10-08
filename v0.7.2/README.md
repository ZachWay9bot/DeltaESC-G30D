# DeltaESC G30D v0.7.2 MOTOR TEST BENCH

Status: **first motor bench candidate only. Wheel off the ground, no road use.**

This branch is based on the CI-green v0.7.0 motor core. It intentionally leaves SHU-recovery development alone and keeps phone-app D0-D9 diagnostics / F0-F5 configuration.

## Motor-critical corrections

- G30 reference mapping is fixed to U/V/W:
  - PA3 / ADC3 = phase U current
  - PA4 / ADC4 = phase V current
  - PA5 / ADC5 = phase W current
  - TIM1 CH1/2/3 = phase U/V/W PWM
- Current polarity follows the ST MCSDK G30 convention: **current = calibrated offset - ADC sample**.
- The old experimental PB1 gate-enable assumption is removed completely.
- PB12 is configured as TIM1 BKIN, active-low, and TIM1 break protection is enabled.
- First-run current is deliberately restricted to **100..500 mA**.
- Default run current is 250 mA; ALIGN uses about 250 mA.
- Software hard-overcurrent threshold is reduced to 100 ADC counts (~5 A with the current 2 mOhm / gain-8 scaling).
- Arm-idle threshold is reduced to 20 counts (~1 A).
- Three-phase plausibility check trips when abs(Iu+Iv+Iw) exceeds 30 counts while armed.
- Fault 0x0C01 = peak-current trip.
- Fault 0x0C02 = phase-sum plausibility trip.
- D9 now returns fault, diagnostic flags, hard-OC threshold and phase-sum threshold.
- Build ID: 0x0720.

## First bench sequence

1. Rear wheel completely free of the ground; 10S pack only.
2. Connect phone app and verify D0 build 0x0720.
3. Run E0 current-offset calibration while disarmed.
4. Read D1/D2/D9. Do not continue with non-zero fault or implausible offsets.
5. Set test current to 100 mA for the first attempt.
6. Use E5 to request the motor sequence. Keep E6 STOP immediately available.
7. Watch D6/D7/D9. Any grinding, wrong-direction kick, current spike, phase-sum fault or observer loss means E6 immediately.
8. Do not increase above 500 mA in this candidate.

This candidate is not a road firmware and CI is not hardware validation.
