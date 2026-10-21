# DeltaESC v0.10.3 — drive source candidate

This is a complete editable source tree derived from the exact v0.10.2
archive in `../downloads/`. It is not a hardware-qualified driving release.
Application base, linker region, IAP source, UART transport, staging and
control metadata are unchanged. The target has **no available ST-Link**.

Build with the historical compiler family:

```bash
make drive-runtime CC=clang-18 OBJCOPY=llvm-objcopy-18 OBJDUMP=llvm-objdump-18
```

The candidate's identifier is `FW_BUILD=0x1030`. Native regression and
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
- Throttle/brake normal stops disconnect all six gate outputs while
  preserving passive ADC triggering. Releasing the controls and acquiring
  a fresh roll can start again. E6, input loss, ADC/PWM/observer/overcurrent
  faults remain terminal stops until reset. Brake always overrides gas.
- Existing limits remain: 12-count rolling-Iq ceiling, 96-count raw hard
  trip and 10% modulation. These nominal low-current bench limits are not
  increased to vehicle power without target measurements.
- Dashboard speed and fault state use live data. Battery remains the
  explicitly documented historical 50% placeholder; BMS/SOC integration
  is pending. Brake presently coasts; regenerative braking is pending
  measured voltage/current constraints. This is not a ride-ready claim.

## Rollback priority

All `iap_*`, `ninebot_link.*`, `stm32f103_min.h` and `linker.ld` are checked
byte-for-byte against v0.10.2. The real, unchanged IAP parser accepts BEGIN
after E6, motor faults and an IAP order error in native integration tests.
Pending IAP blocks motor start. These tests do not erase/program flash or
prove the physical rollback path. There is no new bootloader/CRC/flash-size
or motor-state permission in IAP.

The handler still lives in the replaceable app. A corrupt/non-starting app
or early platform fault can prevent it running; unchanged protocol source
does not establish an independent rescue path. Physical return to stock
without ST-Link remains unverified. No image is published as flash-approved.

Baseline source archive SHA256:
`3a7a4533c743df97f555a199ef603ff52efb34af752301a43ffb0497a3285f6f`.
Baseline v0.10.2 BIN SHA256:
`2b6e26b656a25bdab2b2e98eca447452b66628208cecf956e0df01134cf9eb75`.
