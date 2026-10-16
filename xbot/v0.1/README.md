# Xbot Sensorless Passive BEMF Probe v0.1

**Status: compiled probe, hardware-unvalidated, DO NOT FLASH YET.**

This is a separate Xbot bring-up target. It does not reuse G30-only gate-enable assumptions.

## Xbot schematic mapping

- PA1: V_BAT ADC
- PA3/PA4/PA5: AMP_A / AMP_B / AMP_C
- PA6/PA7/PB1: BEMF_A / BEMF_B / BEMF_C
- PB6/PB7: IOT_TX / IOT_RX, USART1, 115200 8N1
- PA11: PWR_EN / power hold
- PA8/PA9/PA10 and PB13/PB14/PB15: TIM1 bridge PWM pins

PB1 is deliberately treated as ADC9 BEMF_C. It is not a gate-enable pin.

## Hard safety behavior

The probe does not configure TIM1 for PWM, never enables MOE, never enables bridge outputs, never commands torque, and has no Hall dependency. All six PWM pins remain input/pulldown. TIM1 is explicitly reset and its clock remains disabled.

It samples Vbat, three current-amplifier outputs and the three BEMF inputs every millisecond. Phase ordering is converted into a six-sector sequence. Adjacent sector transitions provide direction and electrical-frequency estimates.

IoT UART commands:
- s: one status line
- c: toggle 10 Hz continuous status
- r: reset detector statistics
- ?: help

## Recovery boundary

The app is linked at 0x08001000 for the STM32F103C8T6-class 64 KiB target. It deliberately does not contain the existing DeltaESC G30 staged updater, because that updater uses a 128 KiB flash layout.

Direct UART IAP has been demonstrated on the user's stock DRV420 Xbot. That proves the stock transport, not recovery after replacing the stock application. Before flashing this probe, preserve or reproduce the DRV420 IAP-entry path.

Local verified build facts:
- binary size: 2412 bytes
- binary SHA256: a9b72dd13448742b6bb0567451c67435a6050c6f2222f6f71875c0f79e51922e
- host BEMF detector tests: PASS
- ARM Cortex-M3 clang build: PASS
