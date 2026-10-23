#!/usr/bin/env python3
from pathlib import Path
root=Path(__file__).resolve().parents[1]
main=(root/'src/main.c').read_text(); iap=(root/'src/iap_update.c').read_text(); proto=(root/'src/iap_proto.c').read_text(); link=(root/'src/ninebot_link.c').read_text(); hdr=(root/'src/stm32f103_min.h').read_text()
assert '#define FW_BUILD 0x0605u' in main
assert 'SCB_VTOR=0x08001000u' in main and '#define SCB_VTOR' in hdr
assert '#define MAX_RX_DATA 128u' in iap
assert '#define RX_RING_SIZE 256u' in link
assert 'iap_proto_effective_write_len' in iap
assert 'iap_proto_next_index' in iap
assert 'wire_sum=iap_proto_sum32_update' in iap
assert 'iap_proto_sum32_finish(wire_sum)==supplied' in iap
assert 'ninebot_link_discard_rx()' in main
assert '0x08001002' not in iap
for name in ['DeltaESC_G30D_v0_6_5_ble_syncsafe.bin','DeltaESC_G30D_v0_6_5_ble_observer_zero_vector.bin','DeltaESC_G30D_v0_6_5_ble_sensorless_bench.bin']:
    b=(root/'build'/name).read_bytes(); assert len(b)<=0xD000,(name,len(b))
print('v0.6.5 128-byte IAP + VTOR preflight: PASS')
