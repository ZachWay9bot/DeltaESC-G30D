# DeltaESC G30D v0.7.0 Motor Bench

Base: CI-green v0.6.7 v0.1.x app-compatible freeze.

Focus: motor bring-up only. The dashboard/NinebotCrypto-facing D0-D9 register contract is retained so the already hardware-tested G30 Bench BLE v0.1.1 transport does not need to change.

Motor variants:
- `syncsafe`: power stage cannot arm.
- `voltage_spin_bench`: first active hardware candidate. 200 ms stationary D-axis voltage alignment, then a low-energy rotating q-axis voltage vector at about 2.1% bus modulation. Current feedback does not control PWM. Hard current trip is tightened to 250 ADC counts. Open-loop run auto-stops after 3 seconds.
- `openloop_foc_bench`: D-axis current alignment followed by low-current open-loop FOC. No observer takeover. Auto-stops after 3 seconds.
- `sensorless_candidate`: same startup, then qualified BEMF-observer handover. Closed-loop takeover is impossible until a valid R/L/flux tuple has been supplied.

Motor-core corrections:
- ALIGN uses D-axis current, not Q-axis torque current.
- Three-shunt common-mode current error is removed before Clarke transform.
- F0/F1/F2 R/L/flux settings actually parameterize the motor core.
- PI gains are derived from R/L when valid motor parameters exist.
- Bus-voltage-aware modulation limiting replaces fixed arbitrary PI output scaling.
- First observer qualification uses physically-scaled back-EMF: `v - R*i - L*di/dt`.
- Flux linkage is used as a consistency check between BEMF magnitude and observed electrical speed.
- PA3/PA4/PA5 and TIM1 CH1/2/3 remain A/B/C according to the G30 hardware mapping.
- Current-sensor polarity still requires the first real bench test.

No build here is road validated. First active test is `voltage_spin_bench` only, wheel unloaded/off the ground, before current-loop or sensorless takeover testing.
