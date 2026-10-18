# DeltaESC G30D

## Current public version

# **DeltaESC G30D v0.10.2 ACTIVE TELEMETRY RC**

**Firmware build:** `0x1020`

Experimental sensorless FOC firmware for the original Ninebot G30D Gen1 STM32F103 3-cap ESC.

> [!WARNING]
> **Active bench release candidate, not a road release.**
> This build can drive the power stage. First tests belong on a current-limited, wheel-free bench with ST-Link recovery available.

### Current motor-control path

```text
DISARMED
  -> neutral confirmed
  -> forward roll >= 3 km/h
  -> passive BEMF qualification
  -> PM-flux phase seed
  -> ROLL_VERIFY at Iq = 0
  -> observer confidence checks
  -> bounded Iq ramp
  -> CLOSED sensorless FOC
```

There is **no ALIGN / OPEN_LOOP fallback** in this release.

### Current profile

- Phase resistance: **0.583 ohm**
- Phase inductance: **0.640 mH**
- PM flux linkage: **12.1 mWb**
- Pole pairs: **15**
- Forward BEMF rank step: **5**
- Rolling-start threshold: **3.0 km/h**
- Flux -> FOC frame offset: **+117.15 electrical degrees**
- First-RC torque cap: **12 current ADC counts**
- Hard over-current trip: **96 reconstructed raw counts**

### v0.10.2 telemetry

Two read-only pages preserve useful evidence even after a stop:

- **0xFD / RC12**: FOC state/fault, observer/control phase, phase error, observer step, Id/Iq, rolling verify/ramp counters and observer clamp delta.
- **0xFE / PI12**: phase currents, PWM CCRs, ADC sectors, VBUS raw and safety/fault/gate-kill state.

The common stop path disables the bridge first, then snapshots the last FOC/observer state before resetting the software state machine.

## Source and build

Current release source branch:

**[`release/v0.10.2-active-telemetry-rc`](https://github.com/ZachWay9bot/DeltaESC-G30D/tree/release/v0.10.2-active-telemetry-rc)**

Validated CI run:

**[GitHub Actions run 39159514233](https://github.com/ZachWay9bot/DeltaESC-G30D/actions/runs/39159514233)**

CI artifact:

**[`DeltaESC-G30D-v0.10.2-ACTIVE-TELEMETRY-RC`](https://github.com/ZachWay9bot/DeltaESC-G30D/actions/runs/39159514233/artifacts/12148506283)**

Firmware BIN SHA-256:

```text
2b6e26b656a25bdab2b2e98eca447452b66628208cecf956e0df01134cf9eb75
```

## Repository layout

The default `main` branch intentionally presents **only the current public release**.

Superseded firmware, old bring-up experiments, historical Android builds, intermediate CI workflows and development notes are retained outside the default public landing branch for traceability, but are no longer presented as current firmware.

See [CURRENT_RELEASE.md](CURRENT_RELEASE.md) for release details and [FLASHING.md](FLASHING.md) for the bench flashing notes.

## License

GPL-3.0.
