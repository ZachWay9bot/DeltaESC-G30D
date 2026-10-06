#!/usr/bin/env python3
from pathlib import Path
import hashlib, struct, sys

ROOT = Path(__file__).resolve().parents[1]
items = [
    (ROOT/'build/DeltaESC_G30D_v0_4_pwm_syncsafe.bin', False),
    (ROOT/'build/DeltaESC_G30D_v0_4_zero_vector_active.bin', True),
]

ok = True
for path, active in items:
    b = path.read_bytes()
    sp, reset = struct.unpack_from('<II', b, 0)
    adc_vec = struct.unpack_from('<I', b, (16+18)*4)[0]
    within = len(b) <= 50*1024
    arm_msg = b'ARM: ZERO-VECTOR PWM ACTIVE' in b
    disabled_msg = b'ARM: disabled in SYNC-SAFE build' in b
    checks = {
        'SP': sp == 0x20005000,
        'reset_in_app': 0x08001001 <= reset < 0x0800D800,
        'adc_irq_in_app': 0x08001001 <= adc_vec < 0x0800D800,
        'size_50KiB': within,
        'arm_string_expected': arm_msg == active,
        'safe_string_expected': disabled_msg == (not active),
    }
    print(path.name)
    print(f'  size={len(b)} sha256={hashlib.sha256(b).hexdigest()}')
    print(f'  SP=0x{sp:08X} reset=0x{reset:08X} ADC_IRQ=0x{adc_vec:08X}')
    for k,v in checks.items():
        print(f'  {k}: {"PASS" if v else "FAIL"}')
        ok &= v
sys.exit(0 if ok else 1)
