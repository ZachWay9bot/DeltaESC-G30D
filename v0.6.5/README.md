# DeltaESC G30D v0.6.5 — 128-byte stock IAP + VTOR hardening

Status: **CI candidate only; real-controller flash still unvalidated.**

v0.6.5 is based on the CI-green v0.6.4 stock-IAP-ACK freeze and closes the remaining protocol gaps found during the second full audit before first hardware flash.

## Changes from v0.6.4

- Stock IAP RX payload increased from 64 to **128 bytes**.
- Accepts the stock final 128-byte IAP block with zero padding beyond the advertised firmware length; non-zero tail padding is rejected.
- Explicitly tests and accepts the stock 8-bit block-index wrap `0xFF -> 0x00`.
- Tracks the exact transferred firmware-byte sum and validates the external `CMD 0x09` checksum (`~sum(bytes)`, uint32 LE).
- Encrypted updates must also pass the internal NinebotTEA checksum before the pending-update control block can be committed.
- `SCB_VTOR` is explicitly set to `0x08001000` before interrupts are enabled.
- PA2 RX ring increased to 256 bytes so a complete 128-byte IAP data frame fits without overflow.
- While IAP owns the link, the same bytes are quarantined from the normal app-frame parser.
- SHU ZIP3 is now explicitly **`encryption: encrypted`** and contains `FIRM.bin.enc` only.
- Build identity is `0x0605` / v0.6.5.

## Stock-IAP facts this candidate is built around

- `CMD 0x07`: BEGIN, 4-byte little-endian firmware length.
- `CMD 0x08`: WRITE, 128-byte blocks, block index in ARG/INDEX, index wraps at 8 bits.
- Final WRITE block is zero-padded to 128 bytes by the sender.
- `CMD 0x09`: 4-byte little-endian `~sum(all actual firmware bytes)`.
- `CMD 0x0A`: RESET.
- IAP response uses stock DRV126 framing: `5A A5 00 20 <dst> 0B <result> CK0 CK1`.
- Encrypted image is staged at `0x0800E800` and the pending-update control record is written at `0x0801F800` only after verification.
- Active app begins at `0x08001000`; the stock IAP bootloader at `0x08000000..0x08000FFF` is not overwritten.

## First hardware candidate

Only `DeltaESC_G30D_v0_6_5_SHU_SYNC_SAFE.zip` is intended for the first real test. Its build compiles out power-stage arming and sensorless motor run.

The observer-zero-vector and sensorless-bench binaries remain CI/development outputs. They are **not first-flash artifacts**.

ST-Link is recovery-only. Normal installation/update remains Bluetooth/SHU through the stock dashboard and preserved Ninebot IAP path.
