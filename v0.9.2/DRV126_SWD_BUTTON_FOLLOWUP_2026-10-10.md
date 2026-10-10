# Real G30D SWD Button Follow-up – no MCU input transition on PA12 or PC14

**User-supplied measurements, 2026-10-10**. Original G30D Gen1 3-cap STM32F103 ESC with stock-compatible firmware, connected through STM32CubeProgrammer `-c port=SWD mode=HOTPLUG freq=1000 -r32 ... 0x4`.

| Input port | Released, original Idle capture | Pressed, user confirmed | Difference |
|---|---|---|---|
| GPIOA_IDR, PA12, bit 12 | `0x0000D804`, 1 | `0x0000D804`, 1 | **none** |
| GPIOC_IDR, PC14, bit 14 | `0x00000000`, 0 | `0x00000000`, 0 | **none** |

The operator explicitly confirmed the button was pressed for the PA12 snapshot; PC14 button snapshot also showed no change. 3.29–3.30 V ST-Link target voltage, SWD HOTPLUG recognized real STM32 device ID 0x410.

## Interpretation and correction

1. **Neither PA12 active-low nor PC14 active-high power button has been demonstrated on this actual controller** by GPIO readings under pressing. Do NOT treat PC14 as established based on prior claimed disassembly or schematic interpretation. A single read may miss a short transient; it cannot rule out state-dependent electrical paths or a second upstream power-control network.
2. The original G30 dashboard has a 4-wire interface, reported as: red +5 V after controller starts, black GND, yellow single-wire UART, **green separate power-button switch which grounds that wire on press**. A separate button conductor exists, so it is *not accurate to claim the physical power switch signal is solely a UART frame*. The dedicated switch may affect the **upstream power-latch/regulator circuit**, not necessarily PA12 or PC14 MCU GPIO directly. Its exact connection in this potted controller remains **unmapped**.
3. PA11 remains a measured **push-pull HIGH output** in the running state, and PB1 a measured **analog-mode pin**, based on the previous Idle snapshot. Neither one proves safe behavior at power-up or fault.
4. The existing `v0.9.1` software hardcodes PC14 button. It is **source-only, not safe-to-flash**, and must not be advanced to a physical build as-is based on the inconclusive button-pin mapping. `v0.9.2` is likewise **only a release guard**, not motor hardware enable.
5. Do not repeat timing-critical power-button experiments or reset/unlock/erase procedures merely to force a GPIO result. Continue with safe offline schematic/DRV126 analysis or a qualified, isolated low-voltage button-line test where appropriate.

## Independent references

- [RollerPlausch G30D dashboard lines](https://rollerplausch.com/threads/fingerprint.2502/page-2) documents black GND, yellow UART, green button, red 5 V and a power-regulator input.
- [VescNinebotDash wiring](https://github.com/tonymillion/VescNinebotDash) describes green power-button wire grounded when pressed.
- [RollerPlausch G30D dashboard wiring discussion](https://rollerplausch.com/threads/kontaktverriegelung-montieren-verboten-schluesselschalter-fuer-ninebot-xiaomi.303/page-7) corroborates the separate green/black switch action.

**Result: operator real controller GPIO comparison complete; PA12/PC14 button identity NOT PROVEN; no flash or motor release.**
