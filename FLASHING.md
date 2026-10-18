# Flashing / first bench run

Current firmware: **DeltaESC G30D v0.10.2 ACTIVE TELEMETRY RC**

## ST-Link

Application base:

```text
0x08001000
```

Example STM32CubeProgrammer command:

```powershell
& "C:\\Program Files\\STMicroelectronics\\STM32Cube\\STM32CubeProgrammer\\bin\\STM32_Programmer_CLI.exe" -c port=SWD mode=HOTPLUG freq=1000 -w ".\\DeltaESC_G30D_v0_10_2_active_telemetry_rc.bin" 0x08001000 -v -rst
```

Expected BIN SHA-256:

```text
2b6e26b656a25bdab2b2e98eca447452b66628208cecf956e0df01134cf9eb75
```

## First active test

Use the established current-limited, wheel-free bench setup.

1. Boot with throttle neutral.
2. Roll the wheel forward above roughly 3 km/h.
3. Apply a small throttle command.
4. Expected state progression: `ROLL_VERIFY -> CLOSED`.
5. On any abort, read `0xFD` and `0xFE` before changing the setup.

ST-Link recovery remains the fallback if the experimental build does not boot or communicate as expected.
