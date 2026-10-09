# G30 BLE Recovery DRV126 v0.1.2

Android-only recovery tool for the real G30D Gen1 controller currently running DeltaESC v0.6.5 SYNC-SAFE.

## Hard limits

- BLE/phone only. No ST-Link path.
- Reuses the already hardware-tested v0.1.1 BLE/NinebotCrypto transport.
- Accepts only DeltaESC identity D0 = DESC with build 0x0605.
- Contains one fixed firmware payload: the exact verified stock DRV126 encrypted image.
- The first hardware action is a separate IAP BEGIN test. The full restore button stays locked until CMD 0x07 receives IAP ACK 0x0B with result 0.
- Full restore uses 235 x 128-byte CMD 0x08 blocks, then CMD 0x09 checksum 0xFFC56DAF and CMD 0x0A reset.
- Mutating IAP commands are never automatically retried.
- No motor commands, no configuration writes, no arbitrary file picker.

Bundle SHA-256: 1c01a5610c95de713e6d925a258ef865bffd6f6652cb5ae4d073eeaa3ab2fb1a

Embedded DRV126 encrypted payload SHA-256:
1d41c1259ba5919d923c4c05b489e62a0760dc0003e186378e877a93e8260461
