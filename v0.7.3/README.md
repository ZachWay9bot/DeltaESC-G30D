# DeltaESC G30D v0.7.3 MOTOR TEST BENCH Q15

Status: **bench candidate only; wheel off the ground, 10S, 100..500 mA.**

This branch is based exactly on the frozen CI-green v0.7.2 motor-test bench and changes only the current-loop PI accumulator math.

Why:
The previous integer PI accumulated `(error * Ki) >> 15`. At small current errors used during first bring-up this can round every contribution to zero. The integrator then never builds the correction needed for ALIGN/open-loop low-current control.

v0.7.3:
- keeps all v0.7.2 G30 mapping, PB12 BKIN, current-polarity and phase-sum protections unchanged;
- keeps phone-app D0-D9 / E0 / E5 / E6 / F0-F4 behavior unchanged;
- keeps 100..500 mA test-current hard limits unchanged;
- stores Id/Iq PI accumulators in Q15 units;
- divides only when generating modulation;
- clamps the Q15 accumulators to the same effective integral-output limit;
- adds a host regression that must prove small-error accumulation and PWM bounds;
- build ID becomes 0x0730.

This does not validate motor R/L/flux, observer gains, rotor angle, current scaling or road operation.
