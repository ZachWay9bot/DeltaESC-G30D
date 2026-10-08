# DeltaESC G30D v0.8.1 — transactional DashBLE motor configuration

**SOURCE ONLY. GATES COMPILE-DISABLED. NO FLASH/ROAD RELEASE.**

Built on the CI-green v0.8.0 sensorless/DashBLE integration. This revision makes the F0-F5 motor parameter path safe enough for a future phone UI without allowing partial parameter tuples to perturb the observer.

- F0 R, F1 L, F2 flux, F3 phase offset and F4 test current all require `MAGIC_UNLOCK` and stage into RAM only.
- None of F0-F4 changes the active observer immediately.
- F5 + magic commits only when all five fields are present and within bounds.
- F5 + magic + byte2=0 aborts the pending tuple and restores its shadow from the active RAM values.
- D3/D4/D5 continue to report active R/L/flux. D9 is backward-compatible in its first six bytes and appends phase offset, test current, pending mask and active-valid.
- No parameter persistence to STM32 flash is added. A reboot intentionally returns to defaults.
- PA2/NinebotCrypto, PA12/PA11, dashboard runtime and v0.6.10 IAP retry logic are inherited unchanged.
- The only build target remains SYNC-SAFE with `POWER_STAGE_ARM_ALLOWED=0` and `SENSORLESS_RUN_ALLOWED=0`.

This does not solve the protected stock bootloader recovery hazard and is not a flash authorization.
