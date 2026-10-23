#!/usr/bin/env python3
from pathlib import Path
root=Path(__file__).resolve().parents[1]
main=(root/'src/main.c').read_text(); dash=(root/'src/dashboard_runtime.c').read_text(); iap=(root/'src/iap_update.c').read_text(); mk=(root/'Makefile').read_text()
assert '#define FW_BUILD 0x0607u' in main
assert "g_esc_identity[14]={'D','E','L','T','A','E','S','C','-','G','3','0','D','0'}" in main
assert 'case 0x10u:' in main and 'n=14u;break;' in main
assert 'dashboard_decode_adc' in main and 'dashboard_is_status_request' in main
assert 'f->src==G30_DASH_ADDR&&f->dst==G30_ESC_ADDR' in main
assert '0x08001002' not in iap
assert 'src/dashboard_runtime.c' in mk
for name in ['DeltaESC_G30D_v0_6_7_ble_syncsafe.bin','DeltaESC_G30D_v0_6_7_ble_observer_zero_vector.bin','DeltaESC_G30D_v0_6_7_ble_sensorless_bench.bin']:
    b=(root/'build'/name).read_bytes(); assert len(b)<=0xD000,(name,len(b))
print('v0.6.7 v0.1.x-app compatibility preflight: PASS')
