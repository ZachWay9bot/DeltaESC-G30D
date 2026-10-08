# G30D stock IAP control flow: original 128-KiB dump audit

**Read-only reverse engineering, no stock dump published, no firmware hardware release.**

The user's original G30D stock ESC dump (128 KiB, SHA-256
`9235b466a2f7449b8184560dbef9012d7c12b099e4076100a223ef68d204bb68`)
was analyzed locally. This digest identifies the exact source of the findings
without redistributing proprietary firmware or controller-identifying data.

## Raw observations

- Stock bootloader reset vector: initial SP `0x20000550`, reset `0x08000121`.
- Stock app vector begins at `0x08001000`.
- An additional application vector is present at `0x0800E800` (staging).
- Flash `0x0801F800` contains `{magic=0x0000505A, flag=0, image_size=0}`.
- Flash `0x0801FC00` contains the **same** three 32-bit values.
- Bootloader first 4 KiB has direct 32-bit little-endian literals for
  `0x0801F800` at offsets `0x072C`, `0x07B0`; for `0x0800E800` at
  `0x06C8`; and for `0x08001000` at `0x0648`, `0x06D0`.
- The first 4 KiB has no direct literal for `0x0801FC00`; **this does not
  establish that the second block is unused**. Indirect accesses or app-side
  references remain possible. Do not erase or overwrite it speculatively.

## Disassembly-supported bootloader path

The original Thumb code can be decoded by embedding the first 4 KiB into
an ARM ELF with Clang, then using `llvm-objdump -D`.

- `0x080006F4`: copies 12 bytes from `0x0801F800` into SRAM at
  `0x20000040`; compares the first word to the initialized default magic
  near SRAM `0x20000018`. On mismatch it restores the default 12-byte
  control structure in RAM, then invokes the persistence routine.
- `0x08000788`: invokes the STM32 flash erase helper and writes back the
  12-byte control structure to `0x0801F800`.
- `0x080005AC`: reads control flag at RAM `0x20000044`. A nonzero flag
  enters the staged-copy decision, while zero proceeds toward normal app
  validation.
- `0x0800064C`: checks staged-copy length at RAM `0x20000048`, checks
  the staged application stack vector, erases application pages, and copies
  staged data from `0x0800E800` toward `0x08001000` in chunks.
- `0x08000734`: validates the application stack-pointer region before
  jumping into the application reset vector.

**Important:** The bootloader acts on the control magic and flag. Therefore
writing the magic before the size/flag is genuinely hazardous during a power
cut. The v0.6.8 development guard writes the marker last and invalidates old
control state before staging begins.

## What this does NOT prove

- Complete handling of partial STM32 halfword programming or interruption
  while the *stock bootloader itself* updates its control block.
- The precise independent purpose of the second `0x0801FC00` block.
- Complete integrity/CRC checks across every bootloader path.
- Reliable power-hold, dashboard connection, and SHU reflash/rollback after
  running DeltaESC on the physical ESC.
- Compatibility with another G30D Gen1 bootloader revision.

The v0.6.8 branch is source-only and CI-tested. **Do not flash the original
3-cap ESC on the strength of this audit.**

To reproduce the offset observations without uploading the dump:
`python3 v0.6.8/tools/check_stock_bootloader_layout.py /path/to/esc126_fulldump.bin`
