#!/usr/bin/env python3
"""Offline regression for DeltaESC+EBiCS release safety (no serial I/O)."""
import tempfile
import zipfile
from pathlib import Path
from release_uart_safety_gate import check_text, audit_source_zip, verify_record
from apply_ebics_r9_uart_recovery_fix import patch

def main():
    assert check_text("a.c", "if(boot_crc==0x5e597808u&&flash_kib==128)return 7;")
    assert check_text("g30_update.c", "if(!s||!s->io.compatible_boot)return 7;")
    assert not check_text("safe.c", "if(!stationary)return 6;")
    with tempfile.TemporaryDirectory() as td:
        root = Path(td)
        z = root / "unsafe.zip"
        with zipfile.ZipFile(z, "w") as f:
            f.writestr("src/update.c", "bool ok=(flash_kib==128);")
        assert audit_source_zip(z), "hidden self-lockout in ZIP not detected"
        assert verify_record(root/"MISSING.json", "DeltaESC-G30D", None)
        fw = root / "ebics" / "Firmware"
        (fw/"src").mkdir(parents=True)
        (fw/"include").mkdir()
        (fw/"platform").mkdir()
        (fw/"src/g30_update.c").write_text(
            "if(!s||!s->io.compatible_boot||!s->io.erase||!s->io.write||!s->io.read)return 7;\n"
            "if(!stationary)return 6; G30_STAGE_BASE G30_META_BASE\n")
        (fw/"include/g30_update.h").write_text("typedef struct {\n bool compatible_boot;\n} io;\n")
        (fw/"platform/stm32f103.c").write_text(
            "return a>=G30_STAGE_BASE && a>=G30_META_BASE;\n"
            "return (g30_flash_ops){erase,program,read_flash,0,g30_diag.boot_compatible!=0,0x505au};\n"
            "g30_diag.boot_compatible=(g30_diag.boot_crc==0x5e597808u&&g30_diag.flash_kib==128);\n")
        patch(root/"ebics")
        assert not check_text("src/g30_update.c", (fw/"src/g30_update.c").read_text())
        assert not check_text("platform/stm32f103.c", (fw/"platform/stm32f103.c").read_text())
        try:
            patch(root/"ebics")
        except SystemExit:
            pass
        else:
            raise AssertionError("second patch application must be rejected")
    print("PASS: EBiCS r9 source patch, CRC/size regression, ZIP audit and release default-deny")


if __name__ == "__main__":
    main()
