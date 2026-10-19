#!/usr/bin/env python3
"""Apply the FO91-class IAP deadlock fix to the reconstructed EBiCS r9 tree.

This edits SOURCE ONLY. It does not prove upper Flash capacity, does not build
a binary, does not create an SHU image, and is not permission to flash.
The physical restore qualification gate remains mandatory.
"""
from pathlib import Path
import argparse

def one(text, old, new, file):
    n = text.count(old)
    if n != 1:
        raise SystemExit(f"ABORT: {file}: expected one code anchor, found {n}: {old[:100]}")
    return text.replace(old, new, 1)

def patch(root):
    src = root / "Firmware"
    upd = src / "src/g30_update.c"
    hdr = src / "include/g30_update.h"
    plat = src / "platform/stm32f103.c"
    for p in (upd, hdr, plat):
        if not p.is_file():
            raise SystemExit(f"ABORT: incomplete EBiCS source tree: {p}")
    changes = {}
    s = upd.read_text()
    s = one(s, "!s||!s->io.compatible_boot||!s->io.erase||!s->io.write||!s->io.read",
               "!s||!s->io.erase||!s->io.write||!s->io.read", upd)
    changes[upd] = s
    s = hdr.read_text()
    s = one(s, " bool compatible_boot;\n", "", hdr)
    changes[hdr] = s
    s = plat.read_text()
    s = one(s, "g30_diag.boot_compatible!=0,0x505au", "0x505au", plat)
    s = one(s,
            "g30_diag.boot_compatible=(g30_diag.boot_crc==0x5e597808u&&g30_diag.flash_kib==128);",
            "g30_diag.boot_compatible=0u; /* diagnostic UNKNOWN, never an IAP veto */",
            plat)
    changes[plat] = s
    # Fail-closed: retain all physical Flash write bounds and stationary guard.
    if "if(!stationary)return 6;" not in changes[upd]:
        raise SystemExit("ABORT: mandatory stationary safety guard missing")
    if "G30_STAGE_BASE" not in changes[upd] or "G30_META_BASE" not in changes[upd]:
        raise SystemExit("ABORT: staging/metadata enforcement missing")
    if "G30_STAGE_BASE" not in changes[plat] or "G30_META_BASE" not in changes[plat]:
        raise SystemExit("ABORT: physical Flash address guards missing")
    for p, data in changes.items():
        p.write_text(data)
        print("SOURCE UPDATED:", p)
    print("PASS: EBiCS r9 no longer self-locks IAP due to the boot CRC/128KiB label")
    print("NO FLASH RELEASE. Upper Flash geometry/stock rollback MUST be physically qualified.")

if __name__ == "__main__":
    a = argparse.ArgumentParser()
    a.add_argument("source_tree", type=Path, help="root of reconstructed EBiCS r9 source")
    patch(a.parse_args().source_tree)
