# DeltaESC G30D v0.7.1 - sensorless FOC fixed-point integral audit

**SOURCE-ONLY / bench software verification / GATES COMPILE-DISABLED. NOT A ROAD OR FLASH RELEASE.**

Based on the CI-green `v0.7.0` passive ADC / ST-Link motor probe.
The legacy sensorless FOC code used `acc += (err * Ki) >> 15` with Ki=220:
for error=10 counts, every increment truncated to zero. This mathematically
prevents integration of small currents throughout ALIGN and early open-loop.

The source-only overlay changes the PI integrators to Q15 accumulator units
and divides once for PWM modulation. The historic gains remain PROVISIONAL.
Example simulated zero-current ALIGN: 400 x 10 x 220 = 880000 accumulator
Q15 units, versus 0 in the previous code.

Tests:
- Exact legacy failure reproduced by host test (exit code 3).
- Recompiled Cortex-M3 application is gate-OFF by #error build guard and
  has only a `safe` Makefile target. No active or bench flash target.
- Host test with integer sanitizer checks small-current accumulation,
  saturation, PWM compare range, ZERO vector on disable and a 10000-sample
  randomized synthetic-current stress case.
- Existing v0.7.0 sensorless/ADC probe and Ninebot G30 dashboard/NinebotCrypto
  UART/IAP software regression paths remain.
- Full Cortex-M3 emulator exercises encrypted IAP and communications.

**Still unresolved before real motor operation**: accurate ADC current-channel
and gain/sign mapping; measured ADC offsets/noise; calibrated VBUS ADC divider;
R/L/flux converted to coherent observer units (the current F0/F1/F2 protocol
fields are telemetry/configuration placeholders only); rotor phase alignment;
complementary PWM polarity and dead time on actual gate driver; real ADC PWM
trigger timing; validated fault response and safe supply. Sensorless startup
has NOT been demonstrated on physical hardware.

**Do not flash the original read-protected DRV126 board on CI evidence.**
Removing RDP1 protection destroys flash via mass erase. Check ST-Link option
bytes read-only first, then use an expendable, recoverable test ESC if any
firmware programming is contemplated. The protected stock bootloader's
erase-error recovery limitation remains.
