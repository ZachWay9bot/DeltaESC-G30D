# DeltaESC G30D v0.9.16 – TIM1 BDTR/CCER fail-closed source branch

Status: **SOURCE ONLY. NO FLASH. NO MOTOR TEST.** Built on the actual
`dev/v0.9.14-vesc-method-motor-detect` VESC-method chain, retaining the
`dev/v0.9.15-real-swd-hardware-evidence-audit` hardware findings.

The observed **stock idle** registers were:
- `TIM1_BDTR=0xCD40`: MOE=1, **AOE=1**, BKE=0, BKP=0, LOCK=1, DTG=0x40
- `TIM1_CCER=0x1000`: ADC trigger CH4 on, six MOSFET PWM channel enables off

This register snapshot is **not** the proper live motor-safe template.
STM32 RM0008: `AOE=1` may re-enable `MOE` automatically at the next update
event after a break; `LOCK>=1` freezes BKE/BKP/AOE until MCU reset.

## Actual code changes

1. Adds exhaustive `tim1_bdtr_guard.h` host checks, including 65,536 BDTR
   values, 65,536 CCER values and real-stock `0xCD40` rejection.
2. New `tools/apply_v116.py` overlays the versioned v0.9.14 firmware source,
   checking each patch anchor exactly once. Refuses a pre-existing LOCK,
   AOE, BKE or BKP setting at initial BDTR configuration; a mismatched
   post-write register readback faults before timer start.
3. Runtime `gate_safety_check()` rejects AOE, BKE, BKP, LOCK, altered
   dead time and any unexpected CH1/2/3 enable bits.
4. The existing original `power_stage_force_disarm()` continues to turn off
   all six channel enables and switches driver pins to GPIO-safe inputs.
   **This does not prove real MT8006A HIN/LIN levels.**
5. The passive ADC TIM1-CC4 trigger still has `MOE=1` and `CCER=0x1000`.
   We intentionally **do not** erase MOE in that mode: the injected trigger
   on F103 may depend on it. There is NO MOSFET power enable in this build.
6. `MOTOR_ID_BENCH_ALLOWED=1` now causes a compile error. The
   authenticated BLE `E7` motor command cannot energize this source build.
   `FW_BUILD=0x0916`, so older Android exact-build control guards also
   reject motor commands.

## Independent electrical acceptance (not simulated)

- Confirm real MT8006A HIN/LIN logic, off state at reset, brake, loss
  of throttle, E6, ADC-fault and watchdog with safely accessible low-voltage
  measuring points. **No grounded probe on high-voltage switching nodes.**
- Check PB12/AMP_LIM asserted level, threshold, and reaction. Do not
  set `BKE=1` or `BKP` just because PB12 happens to be the BKIN alternate
  function; the true driver and protection path remains unverified.
- Test gate-off persistence with independent hardware protection and a
  suitably current-limited bench supply. A 10S battery alone is not an
  energy-limited bench supply.

The old v0.9.14 experimental motor-ID bench code is retained as history,
not carried forward as an electrically approved image. Normal NinebotCrypto
communication sources are untouched.
