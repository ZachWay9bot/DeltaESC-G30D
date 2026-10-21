#!/usr/bin/env python3
"""Native tests of the actual candidate source; preserves original IAP bytes."""
import hashlib
import json
from pathlib import Path
import re
import subprocess
import tempfile
import zipfile

ROOT = Path(__file__).resolve().parents[1]
FW = ROOT / 'firmware'
PROTECTED = ['src/iap_update.c', 'src/iap_update.h', 'src/iap_proto.c',
             'src/iap_proto.h', 'src/iap_control.c', 'src/iap_control.h',
             'src/ninebot_link.c', 'src/ninebot_link.h', 'src/stm32f103_min.h',
             'linker.ld']


def run(args, cwd=FW):
    p = subprocess.run(args, cwd=cwd, text=True, capture_output=True)
    if p.returncode:
        raise RuntimeError(f'{args!r}\n{p.stdout}\n{p.stderr}')
    return p.stdout + p.stderr


report = {'hardware_tested': False, 'rollback_physically_verified': False,
          'drive_ready': False, 'protected_source_sha256': {}, 'tests': []}
with zipfile.ZipFile(ROOT / 'downloads/DeltaESC_G30D_v0_10_2_COMPLETE_SOURCE.zip') as original:
    for name in PROTECTED:
        data = (FW / name).read_bytes()
        assert data == original.read(name), f'Unexpected change to protected reference {name}'
        report['protected_source_sha256'][name] = hashlib.sha256(data).hexdigest()
print('PASS: IAP, protocol, control metadata, UART link, CPU header and linker are byte-identical', flush=True)
old = (FW / 'tools/run_v114_tests.sh').read_text()
cases = [x.split() for x in re.findall(r"'([^'\n]+\.c[^'\n]*)'", old)
         if 'host_test.c' in x or x.startswith('test_')]
cases = [x for x in cases if x[0] != 'sensorless_plant_host_test.c']
for entry in [
    'dashboard_runtime_host_test.c dashboard_runtime.c',
    'motor_config_txn_test.c motor_config_txn.c',
    'iap_control_powercut_test.c iap_control.c',
    'test_stock_adc_correlated.c stock_adc_correlated.c stock_adc_timing.c stock_adc_quality.c stock_adc_snapshot.c stock_current_frontend.c',
    'drive_runtime_host_test.c drive_runtime.c',
]:
    cases.append(entry.split())
upstream = [
    'g30_rolling_bemf_host_test.c g30_rolling_bemf.c',
    'g30_passive_bemf_adc_host_test.c g30_passive_bemf_adc.c',
    'rolling_pipeline_host_test.c motor_pipeline.c foc_adc_contract.c stock_current_frontend.c sensorless_control.c vesc_ebics_flux.c',
    'rolling_frame_seed_host_test.c sensorless_control.c vesc_ebics_flux.c stock_current_frontend.c',
]
cases += [s.split() for s in upstream]
with tempfile.TemporaryDirectory(prefix='deltaesc-unit-') as folder:
    for case in cases:
        name = case[0]
        test = ROOT / 'tools/audit_upstream' / name if any(name == s.split()[0] for s in upstream) else FW / 'tools' / name
        deps = list(dict.fromkeys(case[1:] + ['stock_current_frontend.c']))
        exe = Path(folder) / name.removesuffix('.c')
        args = ['gcc', '-std=c11', '-O1', '-g', '-fsanitize=undefined',
                '-fno-sanitize-recover=undefined', '-Isrc']
        if name == 'motor_pipeline_host_test.c' or name.startswith('rolling_'):
            args += ['-DSENSORLESS_CLOSED_LOOP_ALLOWED=1']
        run(args + [str(test)] + ['src/' + d for d in deps] + ['-lm', '-o', str(exe)])
        output = run([str(exe)])
        report['tests'].append({'name': name, 'result': 'PASS', 'output': output})
        print('PASS', name, flush=True)
output = run(['python3', 'tools/test_drive_main.py'], ROOT)
report['tests'].append({'name': 'actual_main_and_iap_integration', 'result': 'PASS', 'output': output})
print(output, end='', flush=True)
main = (FW / 'src/main.c').read_text()
entry = main[main.index('int main(void){'):]
assert entry.index('iap_update_poll();') < entry.index('drive_runtime_poll();')
assert 'g_rolling_bemf.cfg.phase_offset_u16=v.phase_offset;' in main
assert 'case 0xF6u: /* read-only offset calibration' in main
assert 'case 0xFFu: /* measured motor geometry' in main
out = ROOT / 'audit-results/drive-candidate-tests.json'
out.parent.mkdir(exist_ok=True)
out.write_text(json.dumps(report, indent=2) + '\n')
print(f'{len(report["tests"])} C/component/integration groups PASS. No hardware or physical rollback claim.')
