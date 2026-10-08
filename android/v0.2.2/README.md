# Android Link Probe v0.2.2 — proven G30 BLE transport

This delta keeps the v0.2 read-only protocol and safety gates, but replaces the fragile one-shot GATT write path with the behavior observed in the physically successful G30 Bench BLE 0.1.0 test.

Changes:
- actual advertised scooter name is passed into NinebotCrypto;
- no hard-coded NBScooter model name;
- no dependency on MTU enlargement;
- encrypted Ninebot frames are serialized in 20-byte NUS chunks;
- write-with-response only;
- one encrypted frame in flight at a time;
- retry the identical pending chunk after GATT start failure or callback timeout;
- no motor, configuration, IAP or E/F write controls.

Physical reference: G30 Bench BLE 0.1.0 successfully authenticated NBScooter2088, verified MIC, and read ESC registers 0x1A, 0x10 and 0xD0 with zero malformed frames.

Patch SHA-256: 5ab8ab7a389795c26d7049f1e2e6aeadccf72bfb0e2d3cc08bb8b05de98678d2
