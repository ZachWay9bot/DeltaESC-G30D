#!/usr/bin/env python3
from pathlib import Path
import hashlib, struct, sys
ROOT = Path(__file__).resolve().parents[1]
items = [
    (ROOT/'build/DeltaESC_G30D_v0_6_1_ble_syncsafe.bin', False, False),
    (ROOT/'build/DeltaESC_G30D_v0_6_1_ble_observer_zero_vector.bin', True, False),
    (ROOT/'build/DeltaESC_G30D_v0_6_1_ble_sensorless_bench.bin', True, True),
]
ok=True
for path, arm_capable, run_capable in items:
    b=path.read_bytes()
    sp,reset=struct.unpack_from('<II',b,0)
    adc=struct.unpack_from('<I',b,(16+18)*4)[0]
    usart2=struct.unpack_from('<I',b,(16+38)*4)[0]
    checks={
      'SP':sp==0x20005000,
      'reset_in_app':0x08001001<=reset<0x0800E000,
      'adc_irq_in_app':0x08001001<=adc<0x0800E000,
      'usart2_irq_in_app':0x08001001<=usart2<0x0800E000,
      'size_52KiB':len(b)<=52*1024,
      'v061_banner':b'DeltaESC G30D v0.6.1 Stock-Dash BLE SHU-reflash firmware' in b,
      'shu_handoff':b'SHU IAP handoff enabled' in b,
      'stock_app_vector_literal':struct.pack('<I',0x08001002) in b,
      'pa2_halfduplex':b'PA2 USART2 115200 half-duplex stock dashboard' in b,
      'cal_lock_notice':b'AUTO R/L/flux/phase identification E1-E4 intentionally locked' in b,
      'run_banner_expected':(b'BLE E5 may arm/start' in b)==run_capable,
      'zero_banner_expected':(b'BLE E5 arms zero-vector only' in b)==(arm_capable and not run_capable),
      'safe_banner_expected':(b'bridge arming and motor run compiled out' in b)==(not arm_capable),
    }
    print(path.name)
    print(f'  size={len(b)} sha256={hashlib.sha256(b).hexdigest()}')
    print(f'  SP=0x{sp:08X} reset=0x{reset:08X} ADC_IRQ=0x{adc:08X} USART2_IRQ=0x{usart2:08X}')
    for k,v in checks.items():
      print(f'  {k}: {"PASS" if v else "FAIL"}')
      ok &= v

# Exact stock/SHU IAP-start regression vector: 5A A5 + LEN/SRC/DST/CMD/ARG + 4B size + checksum.
body=bytes([4,0x3E,0x20,0x02,0x07,0x00,0x20,0x00,0x00])
chk=(~sum(body)) & 0xFFFF
assert (b"\x5A\xA5"+body+struct.pack("<H",chk)).hex()=="5aa5043e2002070020000074ff"
src=(ROOT/'src/ninebot_link.c').read_text()
assert 'dst == 0x20u' in src and 'arg == 0x07u' in src and 'SHU_MAX_APP_BYTES' in src
assert 'src == 0x21u || src == 0x3Eu || src == 0x3Fu' in src
main_src=(ROOT/'src/main.c').read_text()
assert 'G30_STOCK_APP_VECTOR_BASE + 2u' in main_src
assert 'power_stage_force_disarm();' in main_src

sys.exit(0 if ok else 1)
