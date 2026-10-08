#!/usr/bin/env python3
from __future__ import annotations
import hashlib
import importlib.util
import struct
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

def load_packager():
    p = ROOT / "tools" / "make_shu_zip.py"
    spec = importlib.util.spec_from_file_location("make_shu_zip", p)
    m = importlib.util.module_from_spec(spec)
    assert spec and spec.loader
    spec.loader.exec_module(m)
    return m

def checksum16(data: bytes) -> int:
    return (~(sum(data) & 0xFFFF)) & 0xFFFF

def test_iap_example() -> None:
    # Public/legacy G30 IAP-start example:
    # firmware size 33388 (0x826C), version 0x060D, LEN=8 convention.
    body = bytes([0x08,0x3E,0x20,0x02,0x07,0x6C,0x82,0x0D,0x06])
    chk = checksum16(body)
    frame = b"\x5A\xA5" + body + struct.pack("<H",chk)
    assert frame.hex() == "5aa5083e2002076c820d06d1fe"

def test_ninebottea_regression() -> None:
    pack = load_packager()
    plain = struct.pack("<II",0x20005000,0x08001001) + bytes(range(256))*4
    sp, reset = pack.validate_app_image(plain)
    assert sp == 0x20005000
    assert reset == 0x08001001
    enc = pack.ninebot_tea_encrypt(plain)
    assert len(enc) == 1040
    assert enc[:16].hex() == "e442260569d928a71e88a4f51b0b75d3"
    assert hashlib.md5(enc).hexdigest() == "481406633bae0c36720d6201207c44d4"

def test_source_policy() -> None:
    src = (ROOT/"firmware/src/shu_iap.c").read_text()
    required = [
        "len != 4u && len != 8u",
        "dst != 0x20u",
        "cmd != 0x02u && cmd != 0x03u",
        "arg != 0x07u",
        "fw_size < 256u",
        "power_stage_force_disarm();",
        "g_last_abs_current > IAP_IDLE_COUNTS",
        "irq_disable();",
        "APP_BASE_ADDR + 2u",
        "SCB_AIRCR_SYSRESETREQ",
    ]
    for needle in required:
        assert needle in src, needle

def main() -> int:
    test_iap_example()
    test_ninebottea_regression()
    test_source_policy()
    print("SHU/IAP regression preflight: PASS")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
