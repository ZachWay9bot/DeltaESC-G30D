#!/usr/bin/env python3
from pathlib import Path
import hashlib, struct, sys

ROOT = Path(__file__).resolve().parents[1]
items = [
    ROOT/'build/DeltaESC_G30D_v0_4_2_shu_ble_syncsafe.bin',
    ROOT/'build/DeltaESC_G30D_v0_4_2_shu_ble_activecapable.bin',
]

main_src = (ROOT/'src/main.c').read_text()
ble_src = (ROOT/'src/ninebot_diag.c').read_text()
ble_hdr = (ROOT/'src/ninebot_diag.h').read_text()
iap_src = (ROOT/'src/shu_iap.c').read_text()

source_checks = {
    'PA2_USART2_half_duplex': 'USART2_CR3 = USART_CR3_HDSEL' in ble_src and
                               'GPIOA_BASE, 2u, 0xBu' in ble_src,
    'private_cmd_0x7D': 'DELTAESC_CMD_CONFIG 0x7Du' in ble_hdr,
    'hello_and_diag_only': 'DELTAESC_ARG_HELLO' in ble_src and
                           'DELTAESC_ARG_DIAG' in ble_src and
                           'power_stage_arm' not in ble_src,
    'old_debug_uart_removed': 'uart1_debug_init' not in main_src and
                              'uart_puts' not in main_src,
    'strict_iap_handoff': 'arg != 0x07u' in iap_src and
                          'fw_size < 256u' in iap_src and
                          'APP_MAX_BYTES' in iap_src and
                          'power_stage_force_disarm();' in iap_src and
                          'APP_BASE_ADDR + 2u' in iap_src,
}

ok = all(source_checks.values())
print('source policy')
for k,v in source_checks.items():
    print(f'  {k}: {"PASS" if v else "FAIL"}')

for path in items:
    b = path.read_bytes()
    sp, reset = struct.unpack_from('<II', b, 0)
    adc_vec = struct.unpack_from('<I', b, (16+18)*4)[0]
    checks = {
        'SP': sp == 0x20005000,
        'reset_in_app': 0x08001001 <= reset < 0x0800D800,
        'adc_irq_in_app': 0x08001001 <= adc_vec < 0x0800D800,
        'size_50KiB': len(b) <= 50*1024,
        'old_UART_ARM_text_absent': b'ARM:' not in b,
    }
    print(path.name)
    print(f'  size={len(b)} sha256={hashlib.sha256(b).hexdigest()}')
    print(f'  SP=0x{sp:08X} reset=0x{reset:08X} ADC_IRQ=0x{adc_vec:08X}')
    for k,v in checks.items():
        print(f'  {k}: {"PASS" if v else "FAIL"}')
        ok &= v

sys.exit(0 if ok else 1)
