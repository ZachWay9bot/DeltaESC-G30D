# Xbot Sensorless Passive Probe + UART IAP v0.2

**Status: hardware-unvalidated first-Xbot candidate. Motor output is deliberately impossible in this build.**

v0.2 exists for two jobs only:
1. passively prove the Xbot BEMF/current ADC path with a Hall-less motor, and
2. keep a stock-style Ninebot UART update path alive after the first custom flash.

## Xbot hardware mapping used

From the supplied Xbot schematic:
- PA1 = V_BAT ADC
- PA3/PA4/PA5 = AMP_A/B/C
- PA6/PA7/PB1 = BEMF_A/B/C
- PB6/PB7 = USART1 remap, IoT TX/RX, 115200 8N1
- PA11 = PWR_EN / power hold
- PC14 = PWR_BTN input (not used as a recovery trigger yet)
- PA8/PA9/PA10 = PWM_AH/BH/CH
- PB13/PB14/PB15 = PWM_AL/BL/CL

PB1 is BEMF_C / ADC9. It is never treated as gate enable.

## Hard motor-output safety

- TIM1 is reset and its clock remains disabled.
- MOE is never enabled.
- all six bridge-command pins remain GPIO input/pulldown.
- no commutation, FOC, open-loop ramp or torque command exists in this image.
- BEMF and current channels are ADC-only.

## Flash/update layout

This layout is taken from the verified 128-KiB DRV126 full dump:

- 0x08000000..0x08000FFF: stock bootloader, untouched
- 0x08001000..0x0800E7FF: active application window
- 0x0800E800..0x0801BFFF: staged update window
- 0x0801C000: persistent ESC configuration area, never touched
- 0x0801F800: update-control page used as {0x505A, pending, size}
- 0x0801FC00: second stock control page, deliberately never touched

The application linker hard-stops at 0x0800E800.

## UART IAP compatibility

Target/source IDs and commands follow the Kickscooter/Ninebot path:
- ESC target = 0x20
- PC source = 0x3D
- frame = 5A A5 LEN SRC DST CMD INDEX DATA... CHECKSUM16
- 0x07 BEGIN
- 0x08 WRITE
- 0x09 CRC/finalize
- 0x0A RESET
- 0x0B ACK

Incoming firmware is the stock NinebotTEA/CBC encrypted stream. v0.2 decrypts into staging, validates the application vector and embedded 32-bit checksum, verifies every programmed halfword, and only then writes pending=1 at 0x0801F800. Reset leaves the actual staging->0x08001000 copy to the preserved stock bootloader.

On BEGIN, pending is cleared before staging erase. A power loss during download therefore leaves the currently active app selected.

## Files to use

`XBOT_SENSORLESS_PASSIVE_IAP_v0.2.bin` is the raw application and is **not** the file to select in stock Ninebot IAP.

`ESC2_XBOT_SL02_v0.2_Driver.bin` contains the encrypted Ninebot update stream and is the direct-serial IAP file. The `.bin` suffix and `ESC2_..._Driver` naming are intentional for the classic Ninebot IAP file filter.

`FIRM.bin.enc` contains identical encrypted bytes for SHU-style packaging/reference.

## Identification after boot

The small stock-style command map reports:
- serial: `XBOTSL02V00200`
- DRV version word: 0x0200 -> UI display V2.0.0

This version is only an identifier for the custom Xbot bring-up firmware. It is not a claim that this is a stock Ninebot DRV release.

## Passive diagnostic command

Custom command 0xF0 returns a 44-byte binary telemetry record with Vbat ADC, three current-amplifier ADCs, BEMF A/B/C, common-mode/span, six-sector estimate, direction, transitions and estimated electrical/mechanical speed.

Use:

    py -m pip install pyserial
    py tools/xbot_probe.py COM9

0xF1 resets BEMF detector statistics.

## Validation completed off-device

- Cortex-M3 clang build passes with -Werror.
- application raw size is below the active/staging boundary.
- generated encrypted image decrypts back to the raw image and validates its embedded checksum.
- NinebotTEA fixed test vector passes.
- a known DRV126 SHU FIRM.bin.enc was decrypted and re-encrypted byte-for-byte identically in the local audit.
- host BEMF forward/reverse/noise tests pass.
- source audit rejects any write to TIM1 CCER/BDTR, any use of 0x0801FC00, and verifies PB1 remains analog plus PB6/PB7 USART1 remap.

## Remaining hardware assumption

The Xbot stock DRV 4.2.0 is already proven to accept serial IAP through IoT UART. The supplied observation that SHU stages an image and later copies it to 0x08001000 matches the DRV126 architecture. The exact Xbot bootloader use of 0x0800E800/0x0801F800 is not independently dumped because SWD is inaccessible under epoxy. That is the only remaining recovery-layout assumption before the first custom flash.
