# Current Release

## DeltaESC G30D v0.10.2 ACTIVE TELEMETRY RC

- Public version: **v0.10.2 ACTIVE TELEMETRY RC**
- Internal firmware build: **0x1020**
- Status: **active experimental bench RC**
- Target: **Ninebot G30D Gen1, original 3-cap STM32F103 ESC**
- App base: **0x08001000**
- Firmware BIN size: **32,236 bytes**
- Firmware BIN SHA-256: `2b6e26b656a25bdab2b2e98eca447452b66628208cecf956e0df01134cf9eb75`

### Sensorless path

The active path is rolling-start only:

`ROLL -> ROLL_VERIFY -> CLOSED`

The observer is seeded from passive three-phase BEMF after coherent forward rolling is detected. Verification runs at zero torque before a bounded Iq ramp is allowed.

### Motor profile

| Parameter | Value |
|---|---:|
| R phase | 0.583 ohm |
| Ld | 0.640 mH |
| PM flux | 12.1 mWb |
| Pole pairs | 15 |
| Start threshold | 3.0 km/h |
| Forward rank step | 5 |
| Flux -> FOC offset | +117.15 electrical degrees |
| Initial Iq cap | 12 ADC counts |
| Hard OC | 96 raw counts |

### Diagnostics

- `0xFD / RC12`: live or latched observer/handover state.
- `0xFE / PI12`: current/PWM/ADC and stop/fault context.

### Validation

The complete v0.10.2 reconstruction, host regressions, Cortex-M3 build and flashable active-RC image all passed CI in GitHub Actions run **39159514233**.

The release remains a bench RC until real G30D active hardware testing confirms the complete rolling takeover on the stock ESC power stage.
