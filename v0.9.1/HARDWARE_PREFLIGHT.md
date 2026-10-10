# DeltaESC G30D v0.9.1 / DRV126: hardware preflight

**No custom firmware flash. No motor enable. Original stock firmware only.**

The goal is to capture the REAL hardware configuration, rather than treating a guessed STM32 pin mapping as a motor release. The Cortex-M3 source tests already pass, but they do not prove gate-driver polarity, overcurrent shutdown or motor-current reconstruction.

## First: safety conditions

- Controller on a stable workbench, wheel unloaded and secured; not ridden, not while charging, do not connect/disconnect phases under power. Use the original 10S system, not 14S.
- ST-Link connected to the correct SWD/GND pins, target voltage referenced correctly; do not inject 5V into STM32 3.3V lines.
- Keep the STOCK DRV firmware. Do not choose `Readout Unprotect`, `Full Chip Erase`, `Download`, `Write`, `Option Bytes` or `Start` in STM32CubeProgrammer.
- SWD hotplug should not halt/reset according to ST's manual, but connection itself is still not a guarantee that the scooter won't react unexpectedly. Stop if unexpected switching, motion or a reset occurs.
- **Never use normal/default connect mode to capture these values**: it can reset/halt the target, including its power-latch logic.

## Windows: commands

Use ST's official STM32CubeProgrammer and open PowerShell in the folder containing this file and `G30D_STLink_ReadOnly_Preflight.ps1`.

With scooter powered normally, button NOT pressed:

```powershell
powershell -NoProfile -File .\G30D_STLink_ReadOnly_Preflight.ps1 -Phase Idle
```

Then press the power button only briefly while running **one quick read** of PC14 (avoid a 6-second hold):

```powershell
powershell -NoProfile -File .\G30D_STLink_ReadOnly_Preflight.ps1 -Phase Button
```

Both commands invoke only `STM32_Programmer_CLI.exe -c port=SWD mode=HOTPLUG freq=1000 -r32 <address> 0x4`. If the CLI executable is not on PATH, use `-Cli 'C:\...\STM32_Programmer_CLI.exe'`. Output is saved as timestamped text logs. If the second read shows the same PC14 level despite a real press, the button wiring/level still needs physical checking. Do not guess it away.

## What readings CAN tell us

- `GPIOC_CRH/IDR`: whether PC14 is an input and whether bit 14 changes upon brief press.
- `GPIOA_CRH/ODR`: PA11 output latch direction and observed software output level (NOT its physical 3.3V amplitude or actual power-hold transistor state).
- `GPIOB_CRL/CRH/IDR`: PB1 GPIO configuration, PB12 idle input, PB13/14/15 pin function.
- `TIM1_CCER/BDTR/CCMRx`: actual channel enable, polarity, dead-time, break enable and auto-rearm state in the observed instant.
- `ADCx_JSQR/CR2` and TIM1 CH4: hints about current-sense routing and trigger configuration, not shunt gain or physical sampling accuracy.

## What readings CANNOT tell us

No register capture substitutes for tracing the PCB and measuring actual driver gate polarity, BKIN fault path, BEMF wiring, proper ADC timing, shunt current gain/sign, dead-time and safe motor-off behavior with an oscilloscope and a current-limited supply. A normal stock DRV126 capture cannot automatically be copied to DeltaESC if the controller variant differs.

## Hardware acceptance gates for later motor builds

1. PC14 short press changes signal as documented, PA11 hold remains stable at boot/shutdown; **no bootloop**.
2. All 6 phase signals identified at gate driver, actual polarity and safe inactive state measured with motor output electrically isolated.
3. TIM1 hardware break: BKIN physical route, active polarity, off behavior, `BKE`, `BKP`, `AOE=0` and reset/latched-fault behavior verified experimentally.
4. All 3 currents mapped to ADC pairs; calibrated polarity/current gain and valid 16kHz switching-sector windows proven.
5. Genuine first-spin candidate with independent hardware shutdown. Wheel unloaded, 10S only, gradual current limit and observer R/L/flux validation.

**Existing v0.9.1 source is test-only. The above does not authorize flashing, road riding or a motor power build.**