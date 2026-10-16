# DRV126 IAP reference used by Xbot v0.2

Reference dump: `esc126_fulldump.bin`, 131072 bytes, SHA-256 `9235b466a2f7449b8184560dbef9012d7c12b099e4076100a223ef68d204bb68`.

This document records only the update behavior used as a compatibility reference. It is not a claim that the Xbot DRV420 binary is byte-identical to DRV126.

## Verified DRV126 flash layout

- bootloader: `0x08000000..0x08000FFF`
- active app: starts `0x08001000`
- OTA staging: starts `0x0800E800`
- persistent ESC data: `0x0801C000..0x0801C1FF`
- update-control page: `0x0801F800`
- second stock control page: `0x0801FC00`, purpose unresolved and deliberately not touched by Xbot v0.2

The staged image has a reset vector pointing back into the active-app address range (`0x08001101`), demonstrating that it is stored at `0x0800E800` but linked to execute after copying to `0x08001000`.

## Active-app IAP commands

The active DRV handles:

- `0x07` BEGIN: validates declared encrypted image size, clears pending state, erases staging.
- `0x08` WRITE: accepts indexed encrypted blocks, decrypts NinebotTEA/CBC and writes plaintext into staging.
- `0x09` CRC/finalize: completes the internal plaintext checksum check and marks a valid staged image pending.
- `0x0A` RESET: reboots after the update transaction.
- `0x0B` ACK: carries the result code in the index field.

Result codes observed by the original Ninebot IAP host software:

`0 OK, 1 SIZE, 2 ERASE, 3 WRITEFLASH, 4 UNLOCK, 5 INDEX, 6 BUSY, 7 FORM, 8 CRC, 9 OTHER`.

The classic PC path uses source ID `0x3D`, ESC target `0x20`, 115200 8N1 and 128-byte write packets.

## NinebotTEA/CBC format

Initial 16-byte key:

`FE 80 1C B2 D1 EF 41 A6 A4 17 31 F5 A0 68 24 F0`

IV: eight zero bytes.

TEA uses 32 rounds and delta `0x9E3779B9`. CBC chains the previous ciphertext block. After each 1024 bytes, the key mutates as `key[i] = (key[i] + i) & 0xff`.

The final four plaintext bytes contain the firmware checksum. For the plaintext body before those four bytes:

1. sum all little-endian 32-bit words modulo 2^32,
2. rotate the 32-bit sum left by 16,
3. bitwise invert it.

A known DRV126 SHU `FIRM.bin.enc` was decrypted and re-encrypted byte-for-byte during the local audit. Fixed synthetic vector: plaintext `0001020304050607` -> ciphertext `f3d94c15a303eee0`.

## Bootloader install step

The DRV126 bootloader reads the pending control state at `0x0801F800` and installs the staged image from `0x0800E800` into the active application at `0x08001000` before launching it.

Xbot v0.2 preserves `0x08000000..0x08000FFF`, never writes `0x0801FC00`, and refuses an application that reaches `0x0800E800`.
