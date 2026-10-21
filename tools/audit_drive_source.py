#!/usr/bin/env python3
"""Reproduce v0.10.2 source checks. No device access or release approval."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import tempfile
import zipfile

ARCHIVE_SHA = '3a7a4533c743df97f555a199ef603ff52efb34af752301a43ffb0497a3285f6f'
IMAGE_SHA = '2b6e26b656a25bdab2b2e98eca447452b66628208cecf956e0df01134cf9eb75'
IMAGE = 'DeltaESC_G30D_v0_10_2_active_telemetry_rc.bin'


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def command(args, cwd):
    p = subprocess.run(args, cwd=cwd, text=True, capture_output=True)
    if p.returncode:
        raise RuntimeError(f'{args!r}\n{p.stdout}\n{p.stderr}')
    return p.stdout + p.stderr


def integration_probe(root, temp, cc):
    """Compile the original E5 predicate and armed-mode predicate verbatim.

    Fixed register values exercise Boolean decisions only. This does not
    execute main.c on a processor, model a motor, or qualify gate hardware.
    """
    main = (root / 'src/main.c').read_text()
    e5 = main[main.index('if(cmd==0xE5u)'):]
    predicate = re.search(r'if\((\(commissioning_guard_mask\(\) & ~COMM_GUARD_THROTTLE_NOT_IDLE\)!=0u)\)', e5).group(1)
    armed = re.search(
        r'const uint16_t expected=\(uint16_t\)\(DEADTIME_DTG \|.*?ok=0u;',
        main[main.index('static uint32_t gate_safety_check(void)'):], re.S).group(0)
    rc_flags = (root / 'Makefile').read_text()
    assert '-DMOTOR_ID_BENCH_ALLOWED=0 -DPASSIVE_BEMF_PROBE_ALLOWED=1 -DROLLING_SENSORLESS_RC_ALLOWED=1' in rc_flags
    assert '#define COMM_CURRENT_SCALE_HW_VALID 0u' in main
    assert '#define COMM_ADC_TIMING_HW_VALID 0u' in main
    harness = '''#include <assert.h>
#include <stdio.h>
#include "commissioning_guard.h"
#include "stock_tim1_gate.h"
#include "tim1_bdtr_guard.h"
static unsigned mask;
static unsigned commissioning_guard_mask(void) { return mask; }
#define DEADTIME_DTG 64u
#define MOTOR_ID_BENCH_ALLOWED 0
#define TIM_BDTR(base) (D_GUARD_MOE|D_GUARD_OSSI|D_GUARD_OSSR|DEADTIME_DTG)
#define TIM1_BASE 0
int main(void) {
 commissioning_guard_input_t in={0};
 in.now_ms=100; in.adc_last_ms=100; in.dash_last_ms=100;
 in.adc_stale_ms=5; in.dash_stale_ms=250;
 in.offset_valid=1; in.params_valid=1; in.dash_seen=1;
 in.input_idle_max=3; in.throttle=80; in.control_stopped=1;
 in.vbus_raw=2500; in.vbus_raw_min=50; in.vbus_raw_max=4050;
 in.gate_hw_valid=1;
 mask=commissioning_guard_eval(&in);
 assert(mask==(COMM_GUARD_THROTTLE_NOT_IDLE|COMM_GUARD_CURRENT_SCALE_HW|COMM_GUARD_ADC_TIMING_HW));
 assert(PREDICATE);
 puts("REPRODUCED D1: E5 blocked by current/timing qualification bits");
 uint32_t ccer=STOCK_TIM1_GATE_RUN_CCER,ok=1;
 uint8_t g_id_bench_requested=0;
 ARMED
 assert(ok==0);
 puts("REPRODUCED D2: armed rolling RC rejected by E7-only runtime predicate");
 return 0;
}
'''.replace('PREDICATE', predicate).replace('ARMED', armed)
    p = temp / 'integration_probe.c'
    p.write_text(harness)
    out = temp / 'integration_probe'
    command([cc, '-std=c11', '-Wall', '-Wextra', '-Werror', '-Isrc', str(p),
             'src/commissioning_guard.c', 'src/stock_tim1_gate.c', '-o', str(out)], root)
    result = command([str(out)], root)
    # Trace D3 through actual source, without installing a replacement callback.
    assert 'const motor_pipeline_io_t motor_io={motor_hw_arm,motor_hw_gate_off,' in main
    assert 'static void motor_hw_gate_off(void *ignored) {\n    (void)ignored; power_stage_force_disarm();' in main
    stop = main[main.index('static void power_stage_force_disarm(void) {'):main.index('static void motor_id_bench_coast(void) {')]
    assert 'g_gate_kill_latched=1u;' in stop
    arm = main[main.index('static uint8_t motor_hw_arm(void *ignored) {'):main.index('static void motor_id_hw_gate_off(void *ignored) {')]
    assert 'if (g_gate_kill_latched) return 0u;' in arm
    pipe = (root / 'src/motor_pipeline.c').read_text()
    assert 'brake?MOTOR_STOP_BRAKE:MOTOR_STOP_THROTTLE' in pipe
    assert 'if (p->io.gate_off) p->io.gate_off(p->io_ctx);' in pipe
    result += 'SOURCE TRACE D3: normal throttle/brake stop reaches terminal kill latch\n'
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source-zip', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--build-arm', action='store_true')
    args = parser.parse_args()
    assert sha(args.source_zip) == ARCHIVE_SHA, 'Unexpected source archive; review provenance first'
    repo = Path(__file__).resolve().parents[1]
    cc = os.environ.get('HOST_CC', 'gcc')
    report = {'source_archive_sha256': ARCHIVE_SHA, 'reference_image_sha256': IMAGE_SHA,
              'host_compiler': command([cc, '--version'], repo).splitlines()[0],
              'tests': [], 'hardware_tested': False, 'drive_ready': False,
              'production_source_modified': False}
    with tempfile.TemporaryDirectory(prefix='deltaesc-audit-') as name:
        temp = Path(name)
        root = temp / 'source'
        root.mkdir()
        with zipfile.ZipFile(args.source_zip) as archive:
            for item in archive.infolist():
                dest = (root / item.filename).resolve()
                if not dest.is_relative_to(root.resolve()):
                    raise ValueError('Archive path outside extraction directory')
            archive.extractall(root)
        assert sha(root / 'build' / IMAGE) == IMAGE_SHA
        before = {p.relative_to(root).as_posix(): sha(p) for p in (root / 'src').glob('*') if p.is_file()}
        # Reuse the old C tests against the actual current source. Historical
        # runner filename/grep checks refer to earlier builds and are excluded.
        old = (root / 'tools/run_v114_tests.sh').read_text()
        tests = [x.split() for x in re.findall(r"'([^'\n]+\.c[^'\n]*)'", old)
                 if 'host_test.c' in x or x.startswith('test_')]
        tests = [x for x in tests if x[0] != 'sensorless_plant_host_test.c']
        for case in [
            'dashboard_runtime_host_test.c dashboard_runtime.c',
            'motor_config_txn_test.c motor_config_txn.c',
            'iap_control_powercut_test.c iap_control.c',
            'test_stock_adc_correlated.c stock_adc_correlated.c stock_adc_timing.c stock_adc_quality.c stock_adc_snapshot.c stock_current_frontend.c',
        ]:
            tests.append(case.split())
        upstream = [
            'g30_rolling_bemf_host_test.c g30_rolling_bemf.c',
            'g30_passive_bemf_adc_host_test.c g30_passive_bemf_adc.c',
            'rolling_pipeline_host_test.c motor_pipeline.c foc_adc_contract.c stock_current_frontend.c sensorless_control.c vesc_ebics_flux.c',
            'rolling_frame_seed_host_test.c sensorless_control.c vesc_ebics_flux.c stock_current_frontend.c',
        ]
        for case in upstream:
            tests.append(case.split())
        for case in tests:
            test = case[0]
            supplied = repo / 'tools/audit_upstream' / test
            source = supplied if any(test == x.split()[0] for x in upstream) else root / 'tools' / test
            # Match historical runner, which always links the shared count model.
            deps = list(dict.fromkeys(case[1:] + ['stock_current_frontend.c']))
            out = temp / test.removesuffix('.c')
            cmd = [cc, '-std=c11', '-O1', '-g', '-fsanitize=undefined',
                   '-fno-sanitize-recover=undefined', '-Isrc']
            if test == 'motor_pipeline_host_test.c' or test.startswith('rolling_'):
                cmd += ['-DSENSORLESS_CLOSED_LOOP_ALLOWED=1']
            command(cmd + [str(source)] + ['src/' + x for x in deps] + ['-lm', '-o', str(out)], root)
            output = command([str(out)], root)
            report['tests'].append({'name': test, 'status': 'PASS', 'output': output})
            print('PASS', test, flush=True)
        report['integration_findings'] = integration_probe(root, temp, cc)
        print(report['integration_findings'], end='', flush=True)
        report['arm_build'] = {'status': 'NOT_RUN'}
        if args.build_arm:
            arm_cc = os.environ.get('ARM_CC', 'clang-18')
            report['arm_compiler'] = command([arm_cc, '--version'], root).splitlines()[0]
            build_log = command(['make', '-B', 'rolling-active-rc', f'CC={arm_cc}',
                                 'OBJCOPY=' + os.environ.get('ARM_OBJCOPY', 'llvm-objcopy-18'),
                                 'OBJDUMP=' + os.environ.get('ARM_OBJDUMP', 'llvm-objdump-18')], root)
            built = root / 'build/DeltaESC_G30D_v0_10_1_rolling_active_rc.bin'
            report['arm_build'] = {'status': 'PASS', 'sha256': sha(built),
                                   'size_bytes': built.stat().st_size,
                                   'matches_reference_image': sha(built) == IMAGE_SHA,
                                   'log': build_log}
            assert built.stat().st_size < 53248
            print('ARM BUILD', json.dumps({k: v for k, v in report['arm_build'].items() if k != 'log'}), flush=True)
        after = {p.relative_to(root).as_posix(): sha(p) for p in (root / 'src').glob('*') if p.is_file()}
        assert before == after, 'Audit modified firmware source'
        report['source_file_sha256'] = before
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2) + '\n')
    print(f'{len(report["tests"])} C test groups PASS. D1/D2/D3 reproduced. Drive readiness remains FALSE.')


if __name__ == '__main__':
    main()
