# Full-target UART IAP retry audit — source only

Base: `8b699f426ad43379f6da62a478947aa0b5ae6a27` (v0.6.8 power-loss audit, derived from v0.6.7).

When an accepted WRITE ACK is lost, the sender can retransmit the same block. Previously the receiver rejected that index and entered IAP_ERROR. A repeated CRC after a lost commit ACK likewise failed.

`iap_retry.patch` retains one accepted wire block (128 bytes plus two flags). Only an identical previous-index frame is re-ACKed; it cannot advance the index, change the checksum, decrypt twice, or program flash again. An identical validated CRC is re-ACKed without rewriting the committed control block. BEGIN resets the cached block.

The test executes the complete compiled SYNC_SAFE Cortex-M3 application: boot, USART2 IRQ, two parsers, main loop, reply UART, TEA decryption, staged flash and control commit. It reproduces the defect before applying the patch and verifies the corrected path afterwards.

Checked cases:
- 0x1A, 0x10 and D0–D9 reply framing/length, DESC identity, native dashboard status and refused SAFE arming;
- invalid checksum rejection and subsequent valid read;
- encrypted 128-byte transfer including TEA key rollover, full-size final zero padding, staged plaintext, CRC and RESET;
- 261 data frames with every WRITE repeated, including index 255 -> 0; repeated CRC does not reprogram metadata;
- altered repeated block, wrong index and excessive BEGIN size are rejected without programming;
- v0.6.8 ordering: invalidate old control before staging erase, commit magic last.

Local GCC 13.2.1 SYNC_SAFE audit image: 12248 bytes text, 4 bytes data, 1028 bytes BSS. This build is only for execution tests; it is not a flash artifact. CI also runs the test on the project's pinned Clang 18 build.

**Limits:** MMIO is synthetic. No physical BLE/GATT, actual flash latency, ADC timing, IRQ-preemption timing, motor, or bootloader execution is proven by this test. The unchanged stock bootloader's erase-error hazard remains. This does not diagnose the current SHU `Preparing to flash` timeout: the target firmware has not been identified as installed.

The original motor code and Android transport are unchanged. No SHU firmware ZIP or flash binary is delivered. A green run does not authorize installing on the original controller.

