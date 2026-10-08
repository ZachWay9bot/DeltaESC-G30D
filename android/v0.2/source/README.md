# DeltaESC Tool Android v0.2 PRE-FLIGHT

Read-only diagnostic client for DeltaESC G30D through the original Ninebot G30 dashboard BLE.

## Transport

Android -> Nordic UART Service -> NinebotCrypto legacy transport -> stock dashboard -> yellow single-wire UART -> ESC.

v0.2 performs the 5B/5C/5D dashboard login and verifies the legacy MIC before accepting a received frame.

## Hard safety rule

This v0.2 build is intentionally **READ-ONLY**. The BLE client rejects every ESC CMD 0x02 write before encryption/transmission. The UI exposes only stock firmware read (0x1A) and DeltaESC identity read (D0).

Expected on the completely stock scooter:
- NinebotCrypto/MIC login succeeds;
- 0x1A returns the stock ESC version;
- D0 does **not** match DeltaESC identity;
- no ESC writes are possible.

Expected after the future v0.6.3 SAFE flash:
- D0 must return ASCII DESC and build 0x0603.

No firmware flashing or motor command is performed by this APK.
