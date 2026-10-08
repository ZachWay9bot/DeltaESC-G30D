#!/usr/bin/env python3
from pathlib import Path
root=Path(__file__).resolve().parents[1]
main=(root/'src/main.c').read_text();mk=(root/'Makefile').read_text();txn=(root/'src/motor_config_txn.c').read_text()
assert '#define FW_BUILD 0x0801u' in main
assert 'motor_config_txn_init(&g_motor_cfg,g_cfg_test_current_ma)' in main
assert 'if(!has_magic(f))' in main
assert 'motor_config_stage_u32(&g_motor_cfg,cmd,get_u32(f->payload+2))' in main
assert 'motor_config_pending_complete(&g_motor_cfg)' in main
assert 'motor_config_abort(&g_motor_cfg)' in main
assert 'put_i16(p+6,g_cfg_phase_offset)' in main and 'p[10]=g_motor_cfg.pending_mask' in main
assert 'src/motor_config_txn.c' in mk
assert 'all: safe\n' in mk and 'active: ' not in mk and 'bench: ' not in mk
assert '-DPOWER_STAGE_ARM_ALLOWED=0 -DSENSORLESS_RUN_ALLOWED=0' in mk
assert 'MOTOR_CFG_ALL' in (root/'src/motor_config_txn.h').read_text()
assert 't->active=t->pending' in txn
print('v0.8.1 transactional BLE config preflight: PASS')
