# DeltaESC G30D + EBiCS: mandatory recovery release hold

> **Current target clarification, 21 October 2026:** The owner confirms that ST-Link is unavailable on the DeltaESC target. Historical G30D ST-Link references below do not establish a current recovery option. Rollback without ST-Link has priority; motor development is explicitly authorized. The physical app-independent return path is still unverified.

**No new firmware image, IAP Driver.bin or SHU ZIP is qualified for a sealed UART-only ESC.** All current published v0.10.2 downloads are retained as historical artifacts only.

## DeltaESC v0.10.2

- The exact source ZIP was scanned; the FO91 literal CRC `0x5E597808` / `flash_kib==128` IAP lockout is absent.
- The application IAP stages at `0x0800E800` and commits at `0x0801F800`. A 64-KiB STM32 flash size label is NOT proof that these addresses exist. No speculative erase/program is allowed.
- The original G30D project used a tested and accessible ST-Link. That is **not an option for the potted XBOT**. Do not port the G30D firmware to XBOT on the assumption that stock bootloader, UART and flash geometry match.
- A new source audit script is provided at `tools/release_uart_safety_gate.py`. It rejects FO91-like hard-coded lockouts and denies release without physical restore evidence.

## EBiCS v0.3.4 r7/r8/r9 reconstruction

- r7 and r8 demonstrably use `boot_crc == 0x5E597808 && flash_kib == 128` for IAP eligibility. The r9 reconstruction also retains the same dangerous condition.
- `tools/apply_ebics_r9_uart_recovery_fix.py` applies a **source-only** removal of the CRC and 128-KiB equality veto from the r9 reconstruction, preserving stationery, flash bounds, metadata last, image validation and write verification.
- That code change prevents **one software self-lockout**, but by itself does NOT prove the upper physical flash is present. The new release gate remains closed until geometry and independent serial rescue are verified on matching real hardware.
- The original exact shipped FO91 source is unavailable; do not claim this patch is a byte-identical FO91 repair.

## Release gate

Before any future candidate is published, verify and archive:
1. Exact chip, board, bootloader hash and physical (non-aliased) flash geometry.
2. Physical independent recovery through an actually accessible interface while the active application is invalid.
3. A completed real update then return to correct stock DRV, with readable verified device identity after reset.
4. SHA-256 pinned candidate image, stock image and signed/attested test logs.
5. Current-limited wheel-free motor-safe flashing; no bootloader/config sector damage.

The pending hardware evidence file is deliberately absent. Do not fabricate it. Static tests do not grant flash permission.

**Important:** Existing `.github/workflows/publish-v102-assets.yml` is a legacy auto-publisher and must remain disabled or protected before further release activity. The new CI source safety check does NOT automatically neutralize that pre-existing workflow. Keep release publication on hold.
