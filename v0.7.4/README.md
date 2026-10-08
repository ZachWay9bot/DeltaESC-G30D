# DeltaESC G30D v0.7.4 observer/handover hardening

Status: **bench-only development**. Current limits remain 100..500 mA and all v0.7.2/v0.7.3 safety rules remain unchanged.

Changes:
- observer electrical speed changes from signed 16-bit Q8 to signed 32-bit Q8;
- removes the old ~128 eHz representational ceiling;
- adds a 1/8 IIR filtered observer speed for handover qualification;
- handover now requires **forward** electrical speed for the current forward open-loop sequence, not abs(speed);
- handover rejects excessive raw-vs-filtered speed disagreement;
- D7 keeps its first 8 bytes unchanged, then reports raw and filtered electrical speed as signed whole eHz;
- build ID 0x0740.

This is not a motor-parameter calibration and does not raise torque/current limits.
