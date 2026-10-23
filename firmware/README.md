# DeltaESC v0.10.3 — drive source candidate

This is a complete editable source tree derived from the exact v0.10.2
archive in `../downloads/`. It is not a hardware-qualified driving release.
Application base, linker region, IAP source, UART transport, staging and
control metadata are unchanged. The target has **no available ST-Link**.

Build with the historical compiler family:

```bash
make drive-runtime CC=clang-18 OBJCOPY=llvm-objcopy-18 OBJDUMP=llvm-objdump-18
```

The candidate's identifier is `FW_BUILD=0x1031` (23 October SOC/coast update). Native regression and
actual-main integration checks run from the repository root:

```bash
python3 tools/test_drive_firmware.py
```

The `safe` and `id-bench` targets build the gate-disabled and separate
Motor-ID configurations. Historical `tools/run_v*.sh` and old legacy target
names are retained for provenance; they are not acceptance runners for this
candidate. Use the root test runner above.

## Configuration and behavior

- The `drive-runtime` target starts without donor R/L/flux or donor wheel
  geometry. `sensorless_control_init()` leaves motor parameters invalid.
- `src/g30_target_profile.h` holds the target's measured built-in profile;
  its unknown fields are currently zero. Once actual G30 measurements are
  supplied, a build with that complete profile can operate after power-on
  without rewriting calibration/parameter flash or reconnecting a parameter
  app. Its startup path is tested with explicitly host-only fixture values;
  those fixtures are not inserted into the candidate ARM image.
- Existing F0/F1/F2/F3/F4 stage R/L/flux/phase/current ceiling and F5 commits
  them together. They remain RAM-only. Committing phase updates both the
  powered observer and passive BEMF seed frame.
- New **write FF** payload: little-endian `0xC0DE`, pole-pairs byte,
  circumference in millimetres (`uint16_t` little-endian), total 5 bytes.
  Pole pairs 1..64 and circumference 300..1800 mm are numerical bounds;
  no value is claimed measured by accepting it. Forward electrical phase
  convention remains the existing raw step 5 and must match target wiring.
- **Read FF** returns 12 bytes: `PF13`, pole pairs, raw-forward step,
  circumference LE16, phase offset signed LE16, motor-parameters-valid,
  drive-target-enabled. Existing F6 ADC diagnostics are unchanged.
- Configuration is refused during drive, IAP, calibration or poweroff; in
  the drive target the gas and brake must also be released. This concerns
  motor configuration only and never gates IAP.
- After neutral input and coherent forward rolling, stock dashboard gas
  can request the existing zero-Iq verification and bounded torque ramp.
  Gas proportionally changes the request. Explicit E5 uses the same start
  decision. No ALIGN/open-loop fallback is added.
- In the drive target, startup automatically requests the existing
  512-sample E0 offset calibration after the boot delay, fresh zero gas,
  released brake, fresh ADC data and 100 ms quiet passive BEMF with all six
  gate outputs disconnected. It needs no phone/E0 command. Movement,
  non-neutral input or IAP cancels a partial automatic calibration; existing
  sample-quality/fault checks apply. This measures offsets only; it cannot
  supply the unknown motor R/L/flux, phase frame or wheel geometry.
- Throttle/brake normal stops disconnect all six gate outputs while
  preserving passive ADC triggering. Releasing the controls and acquiring
  a fresh roll can start again. E6, input loss, ADC/PWM/observer/overcurrent
  faults remain terminal stops until reset. Brake always overrides gas.
- Existing limits remain: 12-count rolling-Iq ceiling, 96-count raw hard
  trip and 10% modulation. These nominal low-current bench limits are not
  increased to vehicle power without target measurements.
- Dashboard speed and fault state use live data. Battery SOC is now read
  from stock BMS register 0x32 over its separate USART3 PB10/PB11 full-duplex
  link, 115200 8N1. Only checksum-valid 0x22 -> 0x20 READ_ACK responses with
  a two-byte SOC of 0..100 renew the value. Poll interval is 500 ms; expiry
  is 2000 ms. Before a valid response or after expiry, the dashboard gets 0,
  and read-only B1 distinguishes missing/stale data from a genuine 0% SOC.
  There is no voltage-based guess, BMS configuration write or activation
  command. The stock battery's no-communication behavior is not changed.
- **Owner requirement: no electrical braking and no recuperation.** Brake
  input cancels torque and disconnects the six gate outputs (coast). These
  features are excluded from the scope, not pending work. This software
  result does not establish electrical behavior on the absent controller.

## BMS diagnostics and scheduling

Read **B1** returns 16 bytes: `BM13`, flags (bit0 SOC fresh, bit1 ever seen),
SOC (0 when invalid), accepted replies LE16, age in milliseconds LE32
(`0xFFFFFFFF` before any valid SOC), rejected packets LE16, UART errors LE16.
Legacy B0 SOC and the stock 0x64 dashboard response use this same source.
Existing DA-DF motor diagnostics are unchanged.

USART3 uses a bounded RX ring with byte-arrival timestamps and interrupt TX.
No UART wait loops are added. Partial packets expire after 25 ms; an RX
overrun discards the partial packet. A stuck TX aborts after 20 ms. Its IRQ
priority is below ADC and dashboard/IAP. Main services IAP before this
background task and suspends BMS requests during updater ownership.
BMS data never renew dashboard throttle freshness, arm the bridge or set
an IAP eligibility condition. Protocol and pin mapping are derived from
the existing [SmartESC G30 BMS source](https://github.com/ZachWay9bot/SmartESC_STM32_v21b/blob/b4e7bbb38c9738b0f58bdbd1ff28c61c5d0b279e/common_files/src/task_init.c)
and its STM32 MSP pin setup. Physical verification remains target work.

## Rollback priority

All `iap_*`, `ninebot_link.*`, `stm32f103_min.h` and `linker.ld` are checked
byte-for-byte against v0.10.2. The real, unchanged IAP parser accepts BEGIN
after E6, motor faults and an IAP order error in native integration tests.
Pending IAP blocks motor start. These tests do not erase/program flash or
prove the physical rollback path. There is no new bootloader/CRC/flash-size
or motor-state permission in IAP.

ADC hardware calibration and debug-UART waits are now bounded. An ADC
calibration timeout leaves the bridge latched off and returns to finish
UART/IAP startup; a stuck debug transmitter mutes further debug output.
USART1 is explicitly remapped to PB6/PB7 so it cannot inherit the default
PA9/PA10 route shared with TIM1 gates. USART3 selects PB10/PB11; neither
remap changes the USART2 selection. These application fixes preserve the
existing IAP implementation and flash layout.

The handler still lives in the replaceable app. A corrupt/non-starting app
or early platform fault can prevent it running; unchanged protocol source
does not establish an independent rescue path. Physical return to stock
without ST-Link remains unverified. No image is published as flash-approved.

Baseline source archive SHA256:
`3a7a4533c743df97f555a199ef603ff52efb34af752301a43ffb0497a3285f6f`.
Baseline v0.10.2 BIN SHA256:
`2b6e26b656a25bdab2b2e98eca447452b66628208cecf956e0df01134cf9eb75`.
