#!/usr/bin/env python3
"""Prepare (do not transmit) the G30 RAM-only motor-config frames from measured data.

These frames are ONLY for the matching DeltaESC application after identity
verification. Never use them against stock DRV or an unknown controller.
No persistent parameter/loader/IAP data is written by these commands.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct


def ninebot(src: int, dst: int, cmd: int, arg: int, payload: bytes) -> bytes:
    if len(payload) > 56:
        raise ValueError("payload too long")
    packet = bytes([0x5a, 0xa5, len(payload), src, dst, cmd, arg]) + payload
    check = (~sum(packet[2:])) & 0xffff
    return packet + struct.pack("<H", check)


def build_packets(r_uohm: int, l_nh: int, flux_uwb: int,
                  phase_offset: int, current_ma: int,
                  pole_pairs: int, circumference_mm: int):
    if not 1000 <= r_uohm <= 2_000_000:
        raise ValueError("R outside firmware range")
    if not 1000 <= l_nh <= 5_000_000:
        raise ValueError("L outside firmware range")
    if not 100 <= flux_uwb <= 1_000_000:
        raise ValueError("Flux outside firmware range")
    if not -32768 <= phase_offset <= 32767:
        raise ValueError("phase offset outside int16")
    if not 100 <= current_ma <= 2000:
        raise ValueError("current outside firmware range")
    if not 1 <= pole_pairs <= 64:
        raise ValueError("pole pairs outside firmware range")
    if not 300 <= circumference_mm <= 1800:
        raise ValueError("wheel circumference outside firmware range")
    magic = struct.pack("<H", 0xc0de)
    entries = [
        ("R phase [microohm]", 0xf0, magic + struct.pack("<I", r_uohm)),
        ("L phase [nanohenry]", 0xf1, magic + struct.pack("<I", l_nh)),
        ("PM flux [microweber]", 0xf2, magic + struct.pack("<I", flux_uwb)),
        ("phase offset [u16 ticks as int16]", 0xf3, magic + struct.pack("<h", phase_offset)),
        ("bench current nominal [mA]", 0xf4, magic + struct.pack("<H", current_ma)),
        ("commit RAM config", 0xf5, magic),
        ("pole pairs / circumference RAM", 0xff, magic + struct.pack("<BH", pole_pairs, circumference_mm)),
    ]
    return [(name, ninebot(0x3d, 0x20, 0x03, reg, payload)) for name,reg,payload in entries]


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--r-uohm", type=int, required=True)
    p.add_argument("--l-nh", type=int, required=True)
    p.add_argument("--flux-uwb", type=int, required=True)
    p.add_argument("--phase-offset", type=int, required=True)
    p.add_argument("--current-ma", type=int, required=True)
    p.add_argument("--pole-pairs", type=int, required=True)
    p.add_argument("--circ-mm", type=int, required=True)
    p.add_argument("--output", type=Path)
    a = p.parse_args()
    packets = build_packets(a.r_uohm,a.l_nh,a.flux_uwb,a.phase_offset,
                            a.current_ma,a.pole_pairs,a.circ_mm)
    record = {
        "type": "DeltaESC G30D RAM-only F0-F5/FF request preview",
        "transmitted": False, "hardware_qualified": False,
        "source": "owner-supplied motor measurements only",
        "frames": [{"label": name, "hex": packet.hex("-").upper()} for name, packet in packets],
        "sha256_all_frames": hashlib.sha256(b"".join(f for _,f in packets)).hexdigest()
    }
    result = json.dumps(record,indent=2)+"\n"
    if a.output:
        a.output.write_text(result,encoding="utf-8")
    print(result,end="")


if __name__ == "__main__":
    main()
