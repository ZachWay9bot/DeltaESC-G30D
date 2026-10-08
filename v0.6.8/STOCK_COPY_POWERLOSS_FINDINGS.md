# Original G30D bootloader: copy interrupted/recovery audit

**Source-only investigation. DO NOT FLASH.**

Source checked locally: owner's original 128-KiB `esc126_fulldump.bin`, SHA-256 `9235b466a2f7449b8184560dbef9012d7c12b099e4076100a223ef68d204bb68`. The proprietary dump is not published.

## Confirmed from Thumb disassembly

- Stock reset handler starts at `0x08000121`.
- `0x080005AC`: tests pending-update status and invokes `0x0800064C`.
- `0x0800064C`: staged-copy path. It checks that the staged vector has an SRAM stack pointer, then calls the flash erase helper for `image_size + 0x80` bytes starting at `0x08001000`. The erase helper rounds to 1024-byte pages.
- `0x0800068C..0x080006BC`: copies **256-byte blocks** from staging `0x0800E800` to app `0x08001000`. Helper `0x080003AC` programs 16-bit flash values and reads them back.
- `0x080005C8..0x080005D0`: after normal copy completion, clears pending status and persists it at `0x0801F800`.
- `0x080006D8..0x080006EE`: error in active flash page erase returns code **2**.
- `0x080005F4..0x0800060C`: result code **2** triggers pending status clearing. This is a concrete recovery hazard when one or more app pages are already erased.
- `0x080006A4..0x080006AA`: program or readback failure returns **3**. Code 3 follows the error-report path without the same pending-clear action; a reset/retry is possible *if staging/control data are intact*.

## Local tests (strictly modeled; no hardware)

`v0.6.8/tools/test_stock_copy_faults.py`:
- 6,369 firmware sizes (256..51,200 bytes in eight-byte increments): copy and erase stay below staging; staged image stays below the control pages.
- 650 discrete interruption/reboot cases over selected sizes: retry succeeds **only under the assumptions that staging and pending are intact and physical flash operations have completed**.
- Optional `--dump /local/esc126_fulldump.bin`: confirms layout and direct address literals. Direct references to `0x0801FC00` are absent across the dumped 128 KiB, but indirect references and the second page's purpose are not excluded.

`v0.6.8/tools/test_stock_error_paths.py`:
- Demonstrates an **expected unsafe case**: one app page already erased, subsequent erase reports code 2, pending becomes 0, app vector is erased. A later boot does not automatically retry the staged copy.
- Demonstrates a different outcome for program/readback result 3: pending remains 1 and may permit a new copy attempt.
- With `--dump`, checks exact Thumb opcode signatures for these decisions.

## Honest safety conclusion

The v0.6.8 application-side improvement (invalidate old control before erasing staging, commit magic **last** after verification) is beneficial. It **cannot change** the protected stock bootloader's behavior when its own active-flash erase fails.

In particular, **a blanket Bluetooth-only guaranteed recovery is not possible to assert** with this unmodified bootloader: if it clears pending with an invalid active application, no usable DeltaESC app may remain to receive new BLE/SHU commands.

There is no proof that every power interruption causes the failure case, only that the bootloader has such a failure path. The second 0x0801FC00 metadata page remains untouched and unclassified.

No installation on the valuable 3-cap controller is authorized by a green CI run. Hardware bench qualification and an independently verified recovery strategy remain required.
