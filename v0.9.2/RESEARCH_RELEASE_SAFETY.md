# DeltaESC G30D v0.9.2 – Release-Safety Gate, 10 October 2026

**RESEARCH/TEST SOURCE ONLY. NO FLASH, NO MOTOR DRIVE, NO 60-V APPROVAL.**

Target confirmed by owner: original G30D Gen1 STM32 3-cap controller, MT8006A drivers, epoxy/potting, reinforced MOSFETs and bus capacitors, no ADC divider modification; original/CFW runs star/delta at 60 V. ST-Link and oscilloscope available, **no current-limited bench supply**.

## New cross-check findings

1. **Do not mix generic M365 pinout into this specific G30D design.** M365ESC legacy docs: PC14 power button / PC15 hold; ES2ESC docs: PA12 button / PA11 hold. The earlier user's *specific 3-cap G30D schematic + DRV126 disassembly* instead report PC14 button, PA11 hold, PA12 unconnected and PB1 BEMF_C; original code locations: 0x080052B8, 0x0800411C, 0x080042DC (PC14 init/reads) and 0x080052EE/0x080051D8 (PA11 hold). These claims are strong **source-level board-specific leads**, not an actual pin-voltage measurement. Generic pinout tables are not authority for this exact variant.
2. **MT8006A is NOT an integrated 3-phase overcurrent manager.** Mosway's 8-pin high/low-side driver exposes HIN/LIN, HO/LO, VCC/COM and VB/VS. The A version's low-side output follows LIN; high-side HO follows HIN. No published independent current measurement, fault latch, or standalone multi-phase global enable in the referenced pin description. Actual external amplifier/comparator and TIM1_BKIN (PB12 candidate AMP_LIM) path must be proved separately.
3. **Current v0.9.1 SYNC_SAFE is non-motor and not fully validated even for flashing:** TIM1 CH1–CH3 channel enables disabled, six gate GPIO inputs with pull-downs, BUT TIM1_BDTR.MOE is set for internal CH4 ADC trigger and TIM1_BDTR.BKE is not set. A compilation test alone cannot prove the electrical HIN/LIN levels, pull-down effectiveness or a hardware-break path.
4. **v0.9.1 ADC current scaling/polarity and ADC trigger placement remain diagnostic only**, as do R/L/flux parameters. ADC1 regular VBUS sensing cannot be assumed linear/safe at 60 V without stock divider calibration and rail checks. Stock firmware successfully working at 60 V does not calibrate DeltaESC.
5. SmartESC v2 public README says its G30 variant still requires Hall sensors. That firmware is not a drop-in sensorless release. SHFW is a mature alternative for supported original hardware but must not be represented as a tested 60-V/Star-Delta DeltaESC sensorless solution.

## Enforced guard

`motor_release_gate.py --source <reconstructed-v0.9.1/src/main.c> --purpose diagnostic` succeeds only when the existing motor-on compile vetoes and other conservative markers remain in the real source.

`--purpose motor` must **fail with exit code 2** until actual electrical evidence is gathered and a new separately reviewed motor build is engineered. Do not change the script merely to make a motor release green.

Existing v0.9.1 power button/hold source correction is **kept**, not blindly changed to the generic ES2/PA12 configuration. Do not flash based solely on static results.

## Physical evidence for a future motor release

- Captured stock DRV126 GPIOA / GPIOC input and output configuration, TIM1 CCER/BDTR, voltage and power-hold behavior via read-only SWD. For a potted controller, no assumption that individual MCU pads can be probed.
- Gate HIN/LIN polarity and dead-time at safely accessible low-voltage signals, independent overload/break routing and shutdown latency. No direct earth-referenced scope probing of the powered 60-V bridge.
- Real 10S bus-voltage scale (ADC saturation margin); all three shunt ADC polarity/offset/gain; ADC timing vs PWM sector and whether the third-phase reconstruction is valid.
- Proven motor-off on released throttle, brake and watch-dog fault via disabling *physical gate commands*, not equal 50-% duty as an imaginary freewheel.
- A separate isolated and current-limited power-stage setup before active PWM, plus ST-Link recovery availability. Existing 60-V ride proof is not an appropriate active bring-up environment.
- First controlled 10S wheel-free trial, then 60-V qualification separately; no 60-V sensorless driving until voltage-divider range is checked.

## Sources (and conflicts)

- [etransport M365ESC](https://github-wiki-see.page/m/etransport/ninebot-docs/wiki/M365ESC): M365 PC14/PC15
- [etransport ES2ESC](https://github-wiki-see.page/m/etransport/ninebot-docs/wiki/ES2ESC): legacy Ninebot PA12/PA11
- [DRV126-derived public Ninebot G30 pinout](https://github.com/Vicy-DE/Ninebot-G30-Custom/blob/main/boards/esc-motor/PINOUT.md): PA12/PA11, PB1 claimed enable; conflicts with *this* controller's disassembly + schematic
- [Mosway MT8006A datasheet](https://datasheet4u.com/datasheet-pdf/Mosway/MT8006A/pdf.php?id=1575548)
- [SmartESC v2 README](https://github.com/Koxx3/SmartESC_STM32_v2) (requires Hall)
- [STM32F103 RM0008](https://www.st.com/resource/en/reference_manual/rm0008-stm32f101xx-stm32f102xx-stm32f103xx-stm32f105xx-and-stm32f107xx-advanced-armbased-mcus-stmicroelectronics.pdf)

**Decision:** Keep existing stock firmware on the actual scooter. v0.9.2 is an independent fail-closed release checklist and test; not a flashable safe motor firmware.
