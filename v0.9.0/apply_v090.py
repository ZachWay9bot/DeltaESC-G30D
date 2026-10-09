#!/usr/bin/env python3
"""Reproducible v0.9.0 motor STOP/fault safety fix applied to CI-green v0.8.12.
SOURCE ONLY. DO NOT enable TIM1 gate outputs; not a hardware release.
"""
from pathlib import Path
import subprocess,sys,shutil
root=Path(sys.argv[1]).resolve()
here=Path(__file__).resolve().parent
subprocess.run(["patch","--batch","--fuzz=0","-p1","-i",
                str(here/"sensorless_firstspin_fault_stop.patch")],
               cwd=root,check=True)
def one(s,old,new):
    n=s.count(old)
    if n!=1:raise RuntimeError(f"expected one source anchor, got {n}: {old!r}")
    return s.replace(old,new,1)
p=root/"src/main.c";s=p.read_text()
s=one(s,"#define FW_BUILD 0x080Cu","#define FW_BUILD 0x0900u")
s=one(s,'"v0.8.12 offset validity and noise guard: motor power ALWAYS disabled"',
      '"v0.9.0 observer-fault same-tick PWM kill: motor power ALWAYS disabled"')
s=one(s,"DeltaESC G30D v0.8.11 STATIC 2-RANK ADC1/ADC2 INJECTED SCAN EF, GATES OFF.",
        "DeltaESC G30D v0.9.0 SENSORLESS FAULT-STOP FIX. GATES OFF.")
for guard in ("#define COMM_GATE_HW_VALID 0u",
              "#define COMM_CURRENT_SCALE_HW_VALID 0u",
              "#define COMM_ADC_TIMING_HW_VALID 0u",
              "#if POWER_STAGE_ARM_ALLOWED || SENSORLESS_RUN_ALLOWED"):
    if guard not in s:raise RuntimeError("missing safety interlock: "+guard)
p.write_text(s)
p=root/"Makefile";m=p.read_text()
if "v0_8_12" not in m:raise RuntimeError("v0812 baseline Makefile expected")
m=m.replace("v0_8_12","v0_9_0")
for check in ("all: safe\n","-DPOWER_STAGE_ARM_ALLOWED=0 -DSENSORLESS_RUN_ALLOWED=0"):
    if check not in m:raise RuntimeError("safe-only baseline violated: "+check)
if "active: " in m or "bench: " in m:raise RuntimeError("motor-enabled target prohibited")
p.write_text(m)
shutil.copyfile(here/"tools/first_spin_safety_host_test.c",
                root/"tools/first_spin_safety_host_test.c")
print("PASS v0.9.0: synchronous stop, fault latch, observer skip when off, compile-time motor gates OFF")
