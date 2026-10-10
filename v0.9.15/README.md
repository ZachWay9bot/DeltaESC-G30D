# DeltaESC G30D v0.9.15 – real DRV126 GPIO/TIM1/ADC evidence audit

**OFFLINE ONLY. This is NOT a release of a motor firmware; no user flash requested.**

Confirmed target: original Ninebot G30D Gen1 STM32F103 3-cap potted ESC, MT8006A half-bridge driver ICs, reinforced FETs/capacitors, no ADC modification. ST-Link and oscilloscope available, no current-limited test supply. The scooter runs stock-compatible firmware in star/delta at 60 V; this does **not** qualify DeltaESC for 60 V.

## What is now proven without any more ST-Link

From the user-provided `DeltaESC_DRV126_SWD_Idle_20261010_230709.txt` with 22 real HOTPLUG `-r32` register reads:

- STM32F103/GD variant reported hardware STM32 device ID 0x410, 128 KiB flash, 3.29 V.
- PA11 is GPIO push-pull output HIGH in idle. PB1 is **analog input**, not a gate-enable output. PB12 is floating input HIGH at idle (consistent with, but does not prove, inactive AMP_LIM).
- Six TIM1 FET PWM pins PA8/9/10 and PB13/14/15 are Alternate-Function PP in stock idle. **TIM1 CCER=0x1000**: only CH4 enabled, physical CH1/1N..CH3/3N off. **BDTR=0xCD40**: MOE=1, AOE=1, BKE=0, BKP=0, LOCK=1, DTG=64. That means no TIM1 hardware break **enabled in the captured idle instant**, despite a possible separate analog overcurrent path.
- ADC1 injected sequence is **CH4 (PA4) then CH5 (PA5)**; ADC2 is off in the captured idle state. These do not prove the live FOC sector current measurement configuration.
- Pressed comparison is already present in the project audit: **PA12 stayed HIGH and PC14 stayed LOW**. The reported green dashboard wire connects to an upstream power-button network; neither MCU pin is experimentally confirmed as direct button input. v0.9.1 PC14 hardcoding must NOT be considered hardware-qualified.

From the owner's schematic, PB12 / `AMP_LIM` is tied via a resistor to a pull-up node and an NPN transistor to ground driven by a current sense. This **suggests an active-low current-limit candidate** compatible with STM32 TIM1 BKIN in principle. It does **not** prove the threshold, assert/deassert propagation, gate-driver safety on reset, comparator behavior, or ability to stop a fault before destructive current.

## New reproducible audit

Run in the folder containing `hw_swd_audit.py`:

```sh
python3 -m unittest -v test_hw_swd_audit.py
python3 hw_swd_audit.py /path/to/DeltaESC_DRV126_SWD_Idle_20261010_230709.txt --out actual_report.json
```

The program only parses an existing text file and prints a machine-readable assessment. There are **no MCU writes, no commands to CubeProgrammer, no flashable BIN and no automatic release bypass**. Eight offline unit tests cover button ambiguity, six-gate and ADC decoding, and incomplete data.

## Latest motor code (avoid inventing a new baseline)

`dev/v0.9.14-vesc-method-motor-detect` already contains source-only and an experimental bench `E7` R/L/flux procedure: actual timer/ADC software adapters, host tests, restricted 2.5-second arming, inactive E5 driving. GitHub CI run [38080174652](https://github.com/ZachWay9bot/DeltaESC-G30D/actions/runs/38080174652) passed. That does **not** prove physical power-stage, analog current scaling, battery voltage scaling or any motor operation on the target G30D. Its power-button GPIO is not experimentally verified; preserve this discrepancy as a release veto.

## Remaining evidence required for real motor enable

1. **Real physical coast:** oscilloscope evidence of the **MT8006A HIN/LIN** inactive levels and actual gate-off behavior for brake, throttle release, stop, watchdog, reset and fault. In a potted board, do not probe HV switching nodes with a grounded oscilloscope probe; test at safe accessible low-voltage measurement points with correctly isolated/qualified instruments.
2. **PB12/AMP_LIM safety:** test actual comparator threshold, polarity, reaction, watchdog, and whether a BKIN configuration would reliably inhibit output (`BKE` must not be enabled on guesswork). For future motor-capable TIM1, **AOE=0** and lock semantics must be explicitly configured *before* the first BDTR write, rather than copying the captured `AOE=1,LOCK=1` profile.
3. **Real ADC calibration:** establish shunt gain and channel signs, valid PWM sampling windows and ADC1/ADC2 transition under controlled excitation. Stock idle ADC2-off is not evidence for active FOC dual mode.
4. **10S, not 60V, controlled first activation:** regulated current limit/independent protection, freely spinning wheel, accessible emergency disconnection; no active motor test while there is no energy-limited test supply.
5. **Power-button:** identify how the separate dashboard green lead affects the power latch on THIS potted ESC; do not accept PC14 direct on an unverified assertion.

**Bottom line:** No further ST-Link read is needed for idle, PA12/PC14 pressed, MCU type or PWM pin mode. Another ST-Link *read* is only useful later to capture a different operating state safely; it cannot validate dynamic electrical gate behavior by itself. **No new flash should be performed yet.**