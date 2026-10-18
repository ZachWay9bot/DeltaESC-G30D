# Hardware measurement evidence

These measurements are **XBOT donor-hardware evidence used to qualify the sensorless concepts later ported into DeltaESC**. They are not a claim that DeltaESC v0.10.2 itself has already completed the same powered test sequence on the G30D stock ESC.

## FO60 v0.6.0

Three real takeover runs were recorded with the proven BEMF startup, 100 ms continuous no-coast settle and at most 100 ms of flux-angle dq control at Iq_ref=12.

- **19:17:47: SUCCESS**, observer C, k=-0.6875, 400/400 ticks = 100 ms, max takeover current 67 counts, max phase step 4.31 electrical degrees, mean settle error 6.15 degrees, 0 clamps.
- **19:18:03: SUCCESS**, observer C, k=-0.6875, 400/400 ticks = 100 ms, max takeover current 59 counts, max phase step 3.56 electrical degrees, mean settle error 16.44 degrees, 0 clamps.
- **19:18:10: safely rejected before takeover**, NO_CONFIDENT_OBSERVER, active 0 ticks.

The third run is important evidence too: the confidence gate rejected a bad acquisition before the flux observer was allowed to control PWM.

## FO69 v0.6.9

FO69 extended the already-qualified active interval to 5 seconds.

- five confirmed current-limited, wheel-free 5.0 s runs;
- **4 full successful 20,000/20,000 tick runs**;
- **1 safe CLAMP abort at 10,475 ticks**;
- active max current across the runs: **24..33 ADC counts**;
- no persistent lockout observed.

## FO70 v0.7.0

FO70 was deliberately **passive measurement only** and supplied the rolling-BEMF basis for the later kick-start path: forward sector qualification, direction, speed and freshness while the bridge remained disarmed.

![FO60 takeover quality](assets/fo60_takeover_quality.png)

![FO69 five-second results](assets/fo69_5s_results.png)

![Sensorless hardware progression](assets/sensorless_hardware_progress.png)
