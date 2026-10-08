# DeltaESC v0.6.9 PA12 / stock-IAP safety integration (SOURCE-ONLY)

Base is the exact v0.6.7 app-compatible source with the v0.6.8 power-loss safety overlay.
This is a **separate experimental development branch**, not a merged, installable release.

New source overlay:
- PA2/yellow remains the dashboard communication UART and is not sampled as a button.
- PA12/green: active-low physical power-button input with pull-up.
- PA11: ESC power-hold control output.
- A single long press of 6 seconds initiates a safe power-hold release.
- Stock 0x79 nonzero write sends an acknowledgement before delayed power-off.
- IAP-active inhibits the long-press timer and cancels queued power-off in the main loop.
- Writes use Ninebot commands 0x02, 0x03 without reply, and 0x05 for acknowledgement.
- Stock-shaped versions return 0x0420 while D0 identifies DeltaESC build 0x0609.
- G30 v0.1.x BLE crypto transport and 14-byte ESC identity remain unchanged.
- SYNC-SAFE gates remain disabled. **No flash ZIP will be emitted by the v0.6.9 CI.**

**Known blocking hazard remains**: stock DRV126 bootloader erase-error 2 can clear the
staged-IAP pending flag with an invalid application vector. App-side mitigations do not
solve a bad stock-bootloader copy. A Bluetooth-only guaranteed rollback is not proven.

Test only with off-device host simulations and cross-builds until separate board-level
recovery is proven. A green CI result is NOT permission to flash the original 3-cap ESC.
