#!/usr/bin/env python3
from pathlib import Path
import subprocess,tempfile,re,os,sys
root=Path(sys.argv[1]).resolve()
source=(root/'src/main.c').read_text()
make=(root/'Makefile').read_text()
header=(root/'src/motor_probe.h').read_text()
assert '#error "v0.7.0 motor probe must NEVER arm a physical power stage"' in source
assert 'all: safe\n' in make
assert 'active: ' not in make and 'bench: ' not in make
assert '-DPOWER_STAGE_ARM_ALLOWED=0 -DSENSORLESS_RUN_ALLOWED=0' in make
assert 'motor_probe_capture(s0,s1,s2,s3,' in source
assert 'g_motor_probe' in header and 'MOTOR_PROBE_WINDOW 256u' in header
assert '(3u<<20)|(3u<<0)|(4u<<5)|(5u<<10)|(1u<<15)' in source
assert 'TIM_CCER(TIM1_BASE)=ADC_CCER_MASK' in source
assert 'case 0x10u:' in source and 'case 0xD0u:' in source
b=root/'build/DeltaESC_G30D_v0_7_0_ble_syncsafe.bin'
elf=root/'build/DeltaESC_G30D_v0_7_0_ble_syncsafe.elf'
assert b.is_file() and elf.is_file() and b.stat().st_size<=0xC800
sym=subprocess.check_output(['readelf','-s',str(elf)],text=True)
assert re.search(r'[0-9a-fA-F]{8}\s+92\s+OBJECT\s+GLOBAL.*g_motor_probe',sym)
with tempfile.TemporaryDirectory() as d:
    exe=Path(d)/'probe_test';cc=os.environ.get('HOST_CC','cc')
    subprocess.check_call([cc,'-std=c11','-O2','-Wall','-Wextra','-Werror',
        '-I'+str(root/'src'),str(root/'src/motor_probe.c'),
        str(Path(__file__).with_name('motor_probe_host_test.c')),'-o',str(exe)])
    subprocess.check_call([str(exe)])
print('v0.7.0 passive ADC probe and gate-off host tests: PASS')
