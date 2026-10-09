# G30 Bench BLE Motor v0.3.1

Android companion for DeltaESC G30D **v0.7.6 / build 0x0760**.

The proven G30 BLE/NinebotCrypto transport remains byte-for-byte frozen. The app adds full D0-D9 diagnostics, F0-F4 RAM-only configuration, E0 offset calibration, E5 bench start and E6 STOP.

E5 is locked until the app has confirmed exact build 0x0760, performed E0 in the current session and completed a clean D0-D9 preflight. The firmware auto-disarms after 1000 ms and the app sends a redundant E6 after 1200 ms.

F5 persistence and IAP/SHU update functions are intentionally absent.

Package: `de.deltaesc.motorbench`, version 0.3.1.
