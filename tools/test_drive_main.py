#!/usr/bin/env python3
"""Compile actual target decisions on the host with inert ARM barriers.

Only CPU inline assembly is replaced in temporary source copies. The target
C functions, IAP parser and stop/start call graph are compiled unchanged.
Allocated register memory does not emulate ADC, PWM, flash or a motor.
"""
from pathlib import Path
import re
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def inert_asm(match):
    text = match.group(0)
    if 'mrs %0, primask' in text:
        name = re.search(r'"=r"\((\w+)\)', text).group(1)
        return name + '=0u;'
    return ';'


with tempfile.TemporaryDirectory(prefix='deltaesc-main-test-') as folder:
    temp = Path(folder)
    for source in (ROOT / 'firmware/src').glob('*'):
        if source.is_file():
            text = source.read_text()
            text = re.sub(r'__asm\s+volatile\(.*?\);', inert_asm, text, flags=re.S)
            (temp / source.name).write_text(text)
    harness = ROOT / 'firmware/tools/drive_main_integration_test.c'
    sources = [str(p) for p in temp.glob('*.c') if p.name not in ('main.c', 'aeabi_mem.c')]
    exe = temp / 'main-integration'
    cmd = ['gcc', '-std=c11', '-O1', '-g', '-fsanitize=undefined',
           '-fno-sanitize-recover=undefined', '-Wno-int-to-pointer-cast',
           '-I' + str(temp), '-DPOWER_STAGE_ARM_ALLOWED=1', '-DSENSORLESS_RUN_ALLOWED=1',
           '-DSENSORLESS_CLOSED_LOOP_ALLOWED=1', '-DMOTOR_ID_BENCH_ALLOWED=0',
           '-DPASSIVE_BEMF_PROBE_ALLOWED=1', '-DROLLING_SENSORLESS_RC_ALLOWED=1',
           '-DDRIVE_RUNTIME_ALLOWED=1', str(harness)]
    subprocess.run(cmd + sources + ['-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
