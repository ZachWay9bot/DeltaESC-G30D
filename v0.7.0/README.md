# DeltaESC G30D v0.7.0 – ST-Link passive motor ADC bring-up

**GATES COMPILE-DISABLED, NO PHYSICAL MOTOR START, DO NOT BLIND-FLASH.**

Based on v0.6.10 validated IAP ACK retries, Ninebot v0.1.x BLE identity,
G30 dashboard PA2, PA12 power button, PA11 hold.

What is new: current input ADC zero-offset/noise windows, TIM1 sampling
cycle jitter, 4 kHz max-control timing, and raw ADC registers are captured
in a 92-byte SRAM watch struct `g_motor_probe`.

Assumed injected channels: 3/4/5 phase-current, 1 bus voltage. The
mapping and scaling are *not* yet electrically proven. Neither mechanical
angle nor phase order is identified. Motor R/L/flux remain provisional.
The PM MOSFETs cannot be enabled by this branch, regardless of Bluetooth
commands. This is a source-only development step.

**ST-LINK warning**: If the board is RDP level 1, switching RDP to level 0
erases existing flash. Do not click Readout Unprotect / Unlock / Mass Erase.
Inspect chip/Option Bytes first and independently verify backups.

The original DRV126 bootloader copy/erase recovery hazard remains:
CI green is NOT authorization to overwrite the original G30D firmware.
