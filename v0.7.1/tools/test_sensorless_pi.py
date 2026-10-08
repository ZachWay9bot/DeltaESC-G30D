#!/usr/bin/env python3
"""Off-target math test, never drives motor, requires a GATE-OFF ARM build."""
from pathlib import Path
import subprocess,os,sys,tempfile
root=Path(sys.argv[1]).resolve()
src=(root/'src/sensorless_control.c').read_text()
main=(root/'src/main.c').read_text()
mk=(root/'Makefile').read_text()
assert 'pi_integrate_q15' in src and 'pi_duty_from_q15' in src
assert 'id_int = pi_integrate_q15' in src and 'iq_int = pi_integrate_q15' in src
assert '#error "v0.7.1 FOC PI math audit must NEVER arm a physical power stage"' in main
assert 'all: safe\n' in mk and 'active: ' not in mk and 'bench: ' not in mk
assert '-DPOWER_STAGE_ARM_ALLOWED=0 -DSENSORLESS_RUN_ALLOWED=0' in mk
exe_bin=root/'build/DeltaESC_G30D_v0_7_1_ble_syncsafe.bin'
assert exe_bin.exists() and exe_bin.stat().st_size <= 0xC800
with tempfile.TemporaryDirectory() as d:
    exe=Path(d)/'host_math'
    subprocess.check_call([os.environ.get('HOST_CC','clang'),'-std=c11','-O1','-Wall','-Wextra','-Werror','-Wno-misleading-indentation',
      '-fsanitize=undefined','-fno-sanitize-recover=all','-I',str(root/'src'),
      str(root/'src/sensorless_control.c'),str(Path(__file__).with_name('sensorless_pi_host_test.c')),
      '-o',str(exe)])
    subprocess.check_call([str(exe)])
print('PASS v0.7.1 motor PI math regression and gate-OFF build')
