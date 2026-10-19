#!/usr/bin/env python3
"""Fail-closed release qualification for DeltaESC and EBiCS G30 families.

Software checks only prevent known mistakes. A passing source audit is NOT
proof of a recoverable scooter. Release mode requires independent hardware
rollback evidence for the exact board, bootloader, serial path and image.
"""
import argparse
import hashlib
import json
import re
import sys
import zipfile
from pathlib import Path

BAD_GATES = (
    re.compile(r"boot_crc\s*==\s*0x5e597808", re.I),
    re.compile(r"flash_kib\s*==\s*128\b", re.I),
    re.compile(r"!\s*s->io\.compatible_boot\b"),
)
FIELDS = ("board_profile", "potted", "bootloader_sha256",
          "physical_flash_geometry", "application_image_sha256",
          "stock_image_sha256", "operator_test_log_sha256",
          "restore_transport", "recovery_entrypoint",
          "recovery_independent_of_application",
          "successful_upgrade_and_stock_restore", "hardware_tested_at")
HEX64 = re.compile(r"^[0-9a-fA-F]{64}$")


def check_text(name, data):
    if not name.lower().endswith((".c", ".h", ".cpp")):
        return []
    return [f"{name}: forbidden hardcoded recovery permission: {pat.pattern}"
            for pat in BAD_GATES if pat.search(data)]


def audit_source_zip(path):
    with zipfile.ZipFile(path) as z:
        findings = []
        for f in z.namelist():
            if f.lower().endswith((".c", ".h", ".cpp")):
                findings += check_text(f, z.read(f).decode("utf-8", "replace"))
    return findings


def audit_tree(path):
    findings = []
    for p in path.rglob("*"):
        if p.is_file() and p.suffix.lower() in (".c", ".h", ".cpp"):
            findings += check_text(str(p), p.read_text(encoding="utf8", errors="replace"))
    return findings


def verify_record(path, model, image_sha):
    if not path.is_file():
        return [f"Missing physical UART/restore qualification: {path}"]
    try:
        record = json.loads(path.read_text(encoding="utf8"))
    except (ValueError, OSError) as e:
        return [f"Unreadable qualification record: {e}"]
    errors = [f"Missing {field}" for field in FIELDS
              if field not in record or record[field] in ("", None)]
    for k in ("bootloader_sha256", "application_image_sha256",
              "stock_image_sha256", "operator_test_log_sha256"):
        if not HEX64.fullmatch(str(record.get(k, ""))):
            errors.append(f"Invalid {k} digest")
    if str(record.get("board_profile")) != model:
        errors.append("Wrong physical board profile")
    if image_sha and record.get("application_image_sha256", "").lower() != image_sha.lower():
        errors.append("Physical test did not use exact candidate image")
    if record.get("successful_upgrade_and_stock_restore") is not True:
        errors.append("Real full restore-to-stock test is not PASS")
    if record.get("recovery_independent_of_application") is not True:
        errors.append("Recovery not independent of replaceable application")
    if record.get("potted") is True and record.get("restore_transport") not in (
            "UART_INDEPENDENT", "BOOT_ROM_ACCESSIBLE_UART"):
        errors.append("Potted controller has no independent reachable UART rescue")
    if record.get("potted") is not True and record.get("restore_transport") not in (
            "UART_INDEPENDENT", "BOOT_ROM_ACCESSIBLE_UART", "SWD_VERIFIED"):
        errors.append("Unverified recovery transport")
    if record.get("physical_flash_geometry") not in ("FULL_128K_VERIFIED", "FULL_64K_VERIFIED"):
        errors.append("Physical flash capacity not verified")
    if record.get("physical_flash_geometry") == "FULL_64K_VERIFIED":
        errors.append("Current IAP staging at 0x0800E800 is incompatible with 64KiB flash")
    return errors


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--source-zip", type=Path)
    ap.add_argument("--tree", type=Path)
    ap.add_argument("--release", action="store_true")
    ap.add_argument("--evidence", type=Path, default=Path("evidence/hardware-uart-stock-restore.json"))
    ap.add_argument("--model", choices=("DeltaESC-G30D", "EBiCS-G30D"), default="DeltaESC-G30D")
    ap.add_argument("--image", type=Path)
    a = ap.parse_args()
    errors = []
    if a.source_zip:
        errors += audit_source_zip(a.source_zip)
    if a.tree:
        errors += audit_tree(a.tree)
    if not a.source_zip and not a.tree:
        ap.error("Specify --source-zip or --tree")
    if a.release:
        digest = hashlib.sha256(a.image.read_bytes()).hexdigest() if a.image else None
        errors += verify_record(a.evidence, a.model, digest)
    if errors:
        for e in errors:
            print("BLOCKED:", e, file=sys.stderr)
        return 1
    print("PASS: static source anti-lockout audit", "(NOT a hardware release)" if not a.release else "+ physical evidence")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
