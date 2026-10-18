#!/usr/bin/env python3
from pathlib import Path
import sys
root=Path(sys.argv[1]).resolve()
main=(root/'src/main.c').read_text()
mph=(root/'src/motor_pipeline.h').read_text()
mpc=(root/'src/motor_pipeline.c').read_text()
checks={
 'build-id':'#define FW_BUILD 0x1020u' in main,
 'fd-page':'case 0xFDu:' in main and "p[0]='R';p[1]='C';p[2]='1';p[3]='2';" in main,
 'fe-page':'case 0xFEu:' in main and "p[0]='P';p[1]='I';p[2]='1';p[3]='2';" in main,
 'snapshot-fields':'stop_foc_state' in mph and 'stop_observer_phase' in mph and 'stop_clamp_delta' in mph,
 'gate-off-first':'if (p->io.gate_off) p->io.gate_off(p->io_ctx);' in mpc,
 'snapshot-before-reset':'p->stop_foc_state=p->foc->state;' in mpc and 'sensorless_control_stop(p->foc);' in mpc,
 'rolling-active-retained':'ROLLING_SENSORLESS_RC_ALLOWED' in main and 'motor_pipeline_start_rolling(&g_motor_pipeline' in main,
 'fo70-profile-retained':'#define FO70_R_UOHM 583000u' in main and '#define FO70_FRAME_OFFSET_U16 21327' in main,
}
for k,v in checks.items():
    if not v: raise SystemExit('FAIL '+k)
if mpc.index('if (p->io.gate_off) p->io.gate_off(p->io_ctx);') > mpc.index('p->stop_foc_state=p->foc->state;'):
    raise SystemExit('FAIL snapshot would precede physical gate-off')
print('PASS v0.10.2 contract: active RC retained, gate-off-first snapshot, FD/FE diagnostics linked')
