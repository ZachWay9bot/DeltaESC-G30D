# DeltaESC G30D v0.7.0 motor-core

Status: **development / bench only; not a riding release.**

This branch keeps the authenticated phone-app D0-D9 diagnostics and F0-F5 RAM configuration path while advancing the sensorless motor core. SHU recovery development is intentionally out of scope here.

Changes:
- F0/F1/F2 motor R/L/flux are applied to the observer instead of remaining telemetry-only.
- Current reference is slewed rather than stepped across startup states.
- Sensorless handover requires valid flux, bounded phase error and plausible/stable observer electrical speed.
- D7 adds observer electrical speed in Q8 electrical-Hz.
- Build identity is 0x0700.
- Sync-safe, zero-vector observer and sensorless-bench variants remain separate.

Safety boundary: phase/current mapping, sensor polarity and real G30 motor R/L/flux are still hardware-unvalidated. A green CI build is not motor-run approval.
