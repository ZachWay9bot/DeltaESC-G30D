# DeltaESC G30D v0.8.0 — Sensorless motor and DashBLE integration

**SOURCE-ONLY SOFTWARE CANDIDATE. POWER GATES COMPILE-DISABLED. NOT A FLASH OR ROAD RELEASE.**

This is the unified sensorless motor branch. Built on CI-green v0.7.2
PA2 / NinebotCrypto / dashboard BLE and v0.6.8–0.6.10 IAP safety work,
it imports the independently tested advanced v0.7.0 motor core without
replacing the v0.7.2 BLE, dashboard, power-button or update code.

### What is complete at software level

- 4 kHz three-shunt FOC mathematics with d-axis alignment, rotating
  open-loop vector, qualified BEMF phase/speed lock, blended handover,
  sensorless closed-loop observer and observer-loss fault states
- Measured motor R/L/flux can be supplied as F0/F1/F2 RAM settings
  and now truly enter observer calculations (not just stored fields)
- F3 phase offset and F4 test-current bookkeeping remain
- Corrected undefined signed left shift for negative back-EMF
  measurements discovered by compiler UndefinedBehaviorSanitizer
- D0 reports build 0x0800 and DashBLE v0.3.0 is compatible with it
- D3/D4/D5 report motor settings; D6 now reports BEMF magnitude
- DA–DF expose actual 256-sample ADC diagnostic windows, ADC
  offsets, min/max/raw, TIM1/ADC registers and cycle timing
- PA12 power button, PA11 hold and dashboard firmware interfaces
  are inherited unchanged; ST-Link is emergency recovery only

### CI coverage

- ARM Cortex-M3 safe-only cross-compile, with a negative compile test
  proving powered gates cannot be enabled in this release
- motor-core simulated voltage/open-loop FOC/sensorless variants run
  under UBSan, plus separate DA–DF motor ADC host regression
- actual ARM USART2 emulator with 0x0800 D0, DA–DF read-only pages,
  F0/F1/F2 parameter write ACK and D3/D4/D5 read-back tests
- IAP retransmission, cryptographic framing, dashboard and simulated
  stock bootloader error-path tests
- CI exports source only, excluding any flashable binary or SHU ZIP

### Not complete at hardware level

1. Actual phase-current polarity, shunt gain and order on this ESC are
   unmeasured. The competing motor bench branches made conflicting
   assumptions; none has been proven on this controller.
2. TIM1 six gate pins, complementary polarity, gate-enable/BKIN
   hardware protection and ADC current sample timing are not verified.
3. Voltage divider is not calibrated for higher pack voltages.
4. R/L/flux values must be measured from this particular motor.
5. Stock DRV126 IAP bootloader erase-error recovery cannot guarantee
   BLE rollback. Readout protection may prevent a non-destructive
   backup; ST-Link unlock is not an acceptable blind workaround.
6. The hardware-proven Android DashBLE Motor v0.3.0 is READ-ONLY:
   actual F0–F5 parameter configuration in the Android app remains
   to be added with safe checks. CI synthetic writes are not a
   substitute for testing on the scooter.
7. Real torque, acceleration, no-regen coast, braking, observer
   handover, fault handling and a first spin have not been tested.

**Do not flash this source-derived firmware on the valuable original
G30D ESC. Green software CI does NOT mean it can safely drive a motor.**

### Reproducibility

The motor source overlay, shared as a Git blob at
v0.8.0/motor_core_v070_overlay.tar.xz, is SHA-256 pinned to:
4c76c78595117f8b1ec6f7804f335fd091254fc1febb73281f7e13ef8e611e7c

The integration script checks the motor source hashes, changes only
sensorless_control.c / sensorless_control.h and explicitly scoped
main.c integration points, and never generates an active motor build.
