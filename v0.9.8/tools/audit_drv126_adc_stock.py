#!/usr/bin/env python3
"""Read-only fingerprint check against user's DRV126 full flash dump.

Checks known Thumb instruction bytes and immutable binary SHA-256. This is
an evidence audit, not a motor qualification or substitute for ampere gain.
Does not distribute, rewrite or patch the copyrighted stock firmware.
Usage: python3 tools/audit_drv126_adc_stock.py /path/to/esc126_fulldump.bin
"""
import hashlib
from pathlib import Path
import sys

EXPECTED = '9235b466a2f7449b8184560dbef9012d7c12b099e4076100a223ef68d204bb68'
CHECKS = {
    # Signed three-sector dispatch and the six LUT entries.
    0x0800574E: ('sector TBB', 'dfe80cf0330414142525'),
    # Three applications of MOVW #0xC977, and ARM ASRS #10.
    0x08005766: ('pair 1/6 scale', '4cf6771251438a12'),
    0x08005786: ('pair 2/3 scale', '4cf6771251438a12'),
    0x080057A4: ('pair 4/5 scale', '4cf67713'),
    # ADC pair JSQR TBB branches and input ADC1/ADC2 register addresses.
    0x08005B46: ('JSQR six-sector dispatch', 'dfe805f00804060609090400026000e0'),
    0x08005B34: ('JSQR selector constants', '0d494ff400324ff420334ff4c03407'),
    # Center-aligned TIM1 (CMS=01), ARR=0xFA0, RCR=1.
    0x080048B8: ('TIM1 ARR/RCR init', 'adf822004ff47a60adf8240001258df82850'),
    0x080048D8: ('TIM1 CH4 PWM1 mode=0x60', '6020adf80000'),
    # TIM1 CCER=0x1555, CCR4=0x0F9C.
    0x08005712: ('TIM1 CCER and CCR4 constants', '41f2555340f69c71'),
}

def main() -> int:
    if len(sys.argv) != 2:
        print('usage: audit_drv126_adc_stock.py esc126_fulldump.bin',file=sys.stderr)
        return 2
    p=Path(sys.argv[1]); data=p.read_bytes()
    assert len(data)==131072, 'reference dump length mismatch'
    assert hashlib.sha256(data).hexdigest()==EXPECTED, 'not the referenced DRV126 full dump'
    for address,(label,code) in CHECKS.items():
        b=bytes.fromhex(code)
        actual=data[address-0x08000000:address-0x08000000+len(b)]
        assert actual==b, f'{label} fingerprint mismatch at {address:#x}'
        print(f'PASS {label}: {address:#010x}')
    print('PASS: original DRV126 SHA256 + 9 instruction anchors; no motor power qualification')
    return 0

if __name__=='__main__':
    raise SystemExit(main())