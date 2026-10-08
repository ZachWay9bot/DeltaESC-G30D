# G30 Motor Bench v0.3.1

Android companion for DeltaESC G30D v0.7.2 MOTOR TEST BENCH.

- Preserves the frozen G30 BLE/NinebotCrypto transport byte-for-byte.
- Requires an authenticated DeltaESC D0 signature and **exact build 0x0720** before any write is permitted.
- D0-D9 diagnostic snapshot matches the current v0.7.2 firmware.
- E0 offset calibration, E5 sensorless start, E6 STOP/DISARM.
- F0-F4 RAM configuration for R, L, flux, phase offset and test current.
- Test-current UI and command encoder enforce 100..500 mA.
- F5 is intentionally not exposed because v0.7.2 reports RAM_ONLY.
- No SHU/IAP/update function.

Package ID: `de.deltaesc.motorbench`, so it installs alongside the older probe apps.
