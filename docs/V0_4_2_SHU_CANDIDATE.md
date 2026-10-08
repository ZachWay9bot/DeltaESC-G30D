# v0.4.2 Bluetooth-first SHU candidate

## Purpose

v0.4.2 is the first clean DeltaESC candidate intended to be installed through the stock G30 Bluetooth/dashboard/IAP path rather than by ST-Link.

The first-test package contains the **SYNC-SAFE** firmware only. Power-stage arming is compiled out. The goal is to validate boot, ADC/PWM timing, NinebotCrypto/dashboard transport and the ability to return to the preserved stock bootloader for later SHU updates.

## Canonical CI output

Pinned build environment: Ubuntu 24.04 + clang/LLVM 18.1.3.

- `DeltaESC_G30D_v0_4_2_shu_ble_syncsafe.bin`
  - 4360 bytes
  - SHA-256 `47b7b58f4a897a241392ac752111b008d7e9b2d820b28b482fc02599206ee305`
  - MD5 `098df3e75aaf86d5d8a721abcb920357`
- `FIRM.bin.enc`
  - 4368 bytes
  - MD5 `20cd05dfb64a2281f79380bb82c60895`
- `DeltaESC_G30D_v0.4.2_SYNC_SAFE_SHU.zip`
  - SHA-256 `edf9a0910319e916ed3bbc1c6f8ddb989f205c1cef6ec82592722d151fb44567`

## SHU ZIPv3

Package root contains exactly:

- `FIRM.bin`
- `FIRM.bin.enc`
- `info.json`
- `params.txt`

Metadata:

- model: `max`
- type: `DRV`
- compatible: `max_DRV_STM32F103CxT6`
- encryption: `both`
- application base: `0x08001000`
- maximum application size: 50 KiB, ending before `0x0800D800`

## Update/recovery handoff

A running v0.4.2 watches the raw PA2 dashboard link in parallel with the read-only diagnostic protocol. It accepts an IAP handoff only when all of these are true:

1. header is exactly `5A A5`;
2. LEN is one of the two known public G30 IAP-start conventions, 4 or 8;
3. destination is ESC `0x20`;
4. source is BLE/app/PC `0x21`, `0x3E` or `0x3F`;
5. command is `0x02` or `0x03`, argument `0x07`;
6. firmware size is 256..51200 bytes;
7. 16-bit Ninebot checksum is valid.

After a valid request DeltaESC:

1. forces PB1/gate-driver disabled and TIM1 bridge outputs high-Z;
2. waits 20 ms;
3. refuses handoff if the phase-current residual is above 120 ADC counts;
4. programs only the upper half-word of the application initial stack pointer at `0x08001002` to zero;
5. requests a system reset.

The stock bootloader at `0x08000000..0x08000FFF` is never overwritten by this package.

## Validation boundary

The following are CI-validated:

- source builds with `-Wall -Wextra -Werror`;
- application vectors are inside the stock-safe region;
- image is below the 50 KiB limit;
- BLE private protocol remains read-only;
- SHU/IAP sniffer policy is present;
- ZIPv3 contents and MD5 metadata are self-consistent;
- NinebotTEA encrypted image is generated.

The **real-controller behavior of the stock bootloader after the vector-invalidation handoff is still hardware-unvalidated**. For that reason v0.4.2 is a bench candidate, not a road release. ST-Link is recovery-only if the preserved IAP path does not behave as expected.
