#!/usr/bin/env python3
from pathlib import Path
root=Path(__file__).resolve().parents[1]
main=(root/'src/main.c').read_text(); dash=(root/'src/dashboard_runtime.c').read_text(); iap=(root/'src/iap_update.c').read_text(); mk=(root/'Makefile').read_text()
assert '#define FW_BUILD 0x0606u' in main
assert 'dashboard_decode_adc' in main and 'dashboard_is_status_request' in main
assert 'ninebot_send_app_response(f,G30_DASH_CMD_STATUS,0x00u,p,G30_DASH_STATUS_BYTES)' in main
assert 'f->src==G30_DASH_ADDR&&f->dst==G30_ESC_ADDR' in main
assert 'f->payload[1]' in dash and 'f->payload[2]' in dash
assert 'out[0] = 0x01u' in dash and 'out[1] = 50u' in dash
assert '0x08001002' not in iap
assert 'src/dashboard_runtime.c' in mk
for name in ['DeltaESC_G30D_v0_6_6_ble_syncsafe.bin','DeltaESC_G30D_v0_6_6_ble_observer_zero_vector.bin','DeltaESC_G30D_v0_6_6_ble_sensorless_bench.bin']:
    b=(root/'build'/name).read_bytes(); assert len(b)<=0xD000,(name,len(b))
print('v0.6.6 dashboard-runtime preflight: PASS')
