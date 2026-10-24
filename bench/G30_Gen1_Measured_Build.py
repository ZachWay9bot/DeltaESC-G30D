#!/usr/bin/env python3
"""Build a measured, boot-self-contained G30 Gen1 DRIVE image.

This tool NEVER communicates with the controller, resets, or flashes it.
It uses the unchanged make target and firmware sources in an isolated
temporary directory; no source/profile/IAP flash layout edits are made.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import shutil
import struct
import subprocess
import tempfile


FIELDS = (
    "r_uohm", "l_nh", "flux_uwb", "phase_offset_u16",
    "pole_pairs", "wheel_circumference_mm", "test_current_ma",
)
LIMITS = {
    "r_uohm": (1000, 2_000_000),
    "l_nh": (1000, 5_000_000),
    "flux_uwb": (100, 1_000_000),
    "phase_offset_u16": (-32768, 32767),
    "pole_pairs": (15, 15),  # real G30 Gen1 motor confirmed by owner
    "wheel_circumference_mm": (300, 1800),
    "test_current_ma": (100, 2000),
}
MACROS = {
    "r_uohm": "G30_PROFILE_R_UOHM",
    "l_nh": "G30_PROFILE_L_NH",
    "flux_uwb": "G30_PROFILE_FLUX_UWB",
    "phase_offset_u16": "G30_PROFILE_PHASE_OFFSET_U16",
    "pole_pairs": "G30_PROFILE_POLE_PAIRS",
    "wheel_circumference_mm": "G30_PROFILE_CIRCUMFERENCE_MM",
    "test_current_ma": "G30_PROFILE_CURRENT_MA",
}


def validate(profile):
    if not isinstance(profile, dict):
        raise ValueError("G30 profile must be a JSON object")
    if profile.get("motor") != "G30 Gen1":
        raise ValueError("motor must be exactly 'G30 Gen1' (do not use FO76 donor values)")
    evidence = profile.get("measurement_evidence")
    if not isinstance(evidence, str) or not evidence.strip():
        raise ValueError("measurement_evidence must name the real measurement/log")
    unknown = set(profile) - set(FIELDS) - {"motor", "measurement_evidence"}
    if unknown:
        raise ValueError("Unknown fields: " + ", ".join(sorted(unknown)))
    for key in FIELDS:
        v = profile.get(key)
        if type(v) is not int or not LIMITS[key][0] <= v <= LIMITS[key][1]:
            raise ValueError(f"{key} must be an integer in range {LIMITS[key]}")
    return profile


def make_profile_flags(profile):
    validate(profile)
    return " ".join(f"-D{MACROS[key]}={profile[key]}" for key in FIELDS)


def build(profile_file: Path, out: Path, repo_root: Path, cc: str, objcopy: str, objdump: str):
    profile = validate(json.loads(profile_file.read_text(encoding="utf-8")))
    source = repo_root / "g30"
    if not (source / "Makefile").is_file():
        raise FileNotFoundError("g30/Makefile missing")
    out.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="g30_gen1_build_") as tmp:
        dst = Path(tmp) / "g30"
        shutil.copytree(source, dst, ignore=shutil.ignore_patterns("build", "__pycache__"))
        args = ["make", "-C", str(dst), "drive-runtime",
                f"CC={cc}", f"OBJCOPY={objcopy}", f"OBJDUMP={objdump}",
                "PROFILE_CFLAGS=" + make_profile_flags(profile)]
        subprocess.run(args, check=True)
        binary = dst / "build/DeltaESC_G30D_v0_10_3_drive_candidate.bin"
        elf = dst / "build/DeltaESC_G30D_v0_10_3_drive_candidate.elf"
        if not binary.is_file() or not elf.is_file():
            raise RuntimeError("G30 drive image not produced")
        raw = binary.read_bytes()
        if len(raw) < 256 or len(raw) > 52 * 1024:
            raise RuntimeError("Invalid G30 app image size")
        sp, reset = struct.unpack_from("<II", raw)
        if not (0x20000000 <= sp <= 0x20005000 and
                0x08001001 <= reset < 0x0800E000 and reset & 1):
            raise RuntimeError("App vectors do not match existing G30 IAP layout")
        sha = hashlib.sha256(raw).hexdigest()
        out_bin = out / "DeltaESC_G30D_v0_10_3_Gen1_measured.bin"
        shutil.copy2(binary, out_bin)
        shutil.copy2(elf, out / "DeltaESC_G30D_v0_10_3_Gen1_measured.elf")
        manifest = {
            "motor": "G30 Gen1",
            "measurement_evidence": profile["measurement_evidence"],
            "measurements": {k: profile[k] for k in FIELDS},
            "image": out_bin.name,
            "size_bytes": len(raw),
            "sha256": sha,
            "app_base": "0x08001000",
            "stack_pointer": f"0x{sp:08x}",
            "reset_handler": f"0x{reset:08x}",
            "build_target": "drive-runtime",
            "iap_source_modified": False,
            "note": "Build only. Not flashed; real controller and rollback remain unverified.",
        }
        (out / "manifest.json").write_text(
            json.dumps(manifest, indent=2, ensure_ascii=False) + "\n",
            encoding="utf-8")
        return manifest


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--profile", required=True, type=Path)
    ap.add_argument("--out", type=Path, default=Path("out/g30_gen1_measured"))
    ap.add_argument("--validate-only", action="store_true")
    ap.add_argument("--cc", default="clang-18")
    ap.add_argument("--objcopy", default="llvm-objcopy-18")
    ap.add_argument("--objdump", default="llvm-objdump-18")
    a = ap.parse_args()
    p = validate(json.loads(a.profile.read_text(encoding="utf-8")))
    if a.validate_only:
        print("VALID:", p["motor"], "15 pp, measurements provided and in firmware ranges")
        return
    root = Path(__file__).resolve().parents[1]
    manifest = build(a.profile, a.out, root, a.cc, a.objcopy, a.objdump)
    print(json.dumps(manifest, indent=2, ensure_ascii=False))


if __name__ == "__main__":
    main()
