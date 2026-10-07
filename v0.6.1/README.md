# DeltaESC G30D v0.6.1 SHU reflash-safe candidate

This branch is reconstructed from the CI-green v0.6 source plus the deterministic v0.6.1 patch.

Key change: a strict stock Ninebot/SHU IAP-start sniffer on the PA2 dashboard link. On an exact checksum-valid ESC update request, DeltaESC fully disarms the bridge, verifies near-zero current, programs only the upper half-word of the application initial stack pointer at 0x08001002 to zero, and resets. The preserved factory 4 KiB bootloader should then reject the invalid application and remain in its recovery/IAP path so SHU can retry and flash the next application.

The factory bootloader at 0x08000000..0x08000FFF is never overwritten by DeltaESC.

CI builds three binaries and three ZIPv3 SHU packages containing FIRM.bin, NinebotTEA FIRM.bin.enc, info.json restricted to max_DRV_STM32F103CxT6, and params.txt.

IMPORTANT: software/CI validation is not the same as real-controller validation. The first SHU -> DeltaESC -> next DeltaESC / stock rollback test must still be performed with ST-Link and a verified full 128 KiB backup available.
