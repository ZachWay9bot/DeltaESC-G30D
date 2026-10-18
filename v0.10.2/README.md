# DeltaESC G30D v0.10.2 – Active RC Telemetry

v0.10.2 keeps the v0.10.1 rolling-active motor behavior unchanged and makes the
first hardware run diagnosable after any stop.

The common motor stop path still kills the physical bridge first. Immediately
after that hardware-off action, it snapshots the FOC/observer state before the
normal software reset changes the live state back to STOP.

## New read-only pages

### 0xFD / RC12
- state and FOC fault
- power-armed flag and pipeline stop reason
- observer and control phase
- phase error and observer phase step
- Id / Iq
- rolling good ticks and Iq-ramp ticks
- observer clamp delta
- last current peak

When the bridge is running the page is live. After a stop it returns the latched
pre-reset snapshot, which is the useful part when a first-spin attempt fails.

### 0xFE / PI12
- last reconstructed Ia/Ib/Ic and peak
- CCR1/2/3
- configured run-Iq count target
- last/current ADC sector
- VBUS raw ADC
- safety latch, pipeline fault latch and terminal gate-kill latch

No power limits, phase convention, motor parameters, observer thresholds, start
threshold or gate timing are changed from v0.10.1.
