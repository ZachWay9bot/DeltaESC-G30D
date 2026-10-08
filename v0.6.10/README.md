# DeltaESC v0.6.10: UART IAP ACK retry integration (source only)

**Build-and-simulate research branch only. NO FLASH IMAGE RELEASE.**

The baseline is v0.6.9 PA12 power-button + v0.6.8 metadata power-loss guard, built by
reconstructing the pinned v0.6.7 mainline with audited overlays.

v0.6.10 integrates the independently tested IAP duplicate-ACK fix from
`fix/v068-iap-retry-audit` on top of v0.6.9:

- A repeat of the most recently accepted encrypted WRITE block, with identical
  index and bytes, is re-ACKed, without running decryption or flash writes again.
- An identical previously accepted CRC after commit is re-ACKed, without
  touching the staging/control pages again.
- Wrong-index or altered duplicate packets still fail closed.
- Previous v0.6.8 control-block invalidation before erasing stage and marker-last
  commit are retained.
- Existing v0.6.9 green PA12 power/hold, Ninebot 0x79, stock 0x02/0x03/0x05
  write semantics, 14-byte BLE v0.1.x identity and native dashboard code
  are retained.

The CI uses Unicorn with synthetic STM32 registers and compiles the *actual
Cortex-M3 binary* for USART2 RX/IRQ/parser, encrypted IAP, CRC, flash writes,
update marker and reset, including 261 WRITE blocks and lost ACKs. The simulator
cannot validate physical brownouts, actual BLE, hardware pins or the stock bootloader.

**Release blocker is unchanged:** the original DRV126 bootloader may clear the
pending update on erase error 2 after invalidating the active app. This can
leave Bluetooth recovery unavailable. No app update can guarantee a safe
Bluetooth-only rollback after that failure.

The GitHub Actions artifact intentionally contains **source only**, never a
SHU ZIP or firmware BIN. This freeze is not for direct flashing, 10S test or
motor operation. An independently demonstrated recovery path on expendable ESC
hardware is required before evaluating a physical firmware update.
