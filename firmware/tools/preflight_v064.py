#!/usr/bin/env python3
from pathlib import Path
root=Path(__file__).resolve().parents[1]
main=(root/'src/main.c').read_text()
iap=(root/'src/iap_update.c').read_text()
proto=(root/'src/iap_proto.c').read_text()
assert '#define FW_BUILD 0x0604u' in main
assert "p[0]='D';p[1]='E';p[2]='S';p[3]='C'" in main
assert 'v0.6.4 G30-framing + staged-IAP firmware' in main
assert 'iap_proto_build_ack(dst,result,b)' in iap
assert 'out[2] = 0u' in proto
assert 'out[5] = IAP_CMD_ACK' in proto
assert 'out[6] = result' in proto
assert '#define IAP_CMD_ACK 0x0Bu' in (root/'src/iap_proto.h').read_text()
assert '0x08001002' not in iap
for name in ['DeltaESC_G30D_v0_6_4_ble_syncsafe.bin','DeltaESC_G30D_v0_6_4_ble_observer_zero_vector.bin','DeltaESC_G30D_v0_6_4_ble_sensorless_bench.bin']:
    b=(root/'build'/name).read_bytes()
    assert len(b) <= 0xD000, (name,len(b))
print('v0.6.4 G30 framing + stock IAP ACK preflight: PASS')
