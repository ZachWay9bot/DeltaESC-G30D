#!/usr/bin/env python3
"""Static DeltaESC G30D power-stage release gate.

This is an additional proof obligation, NOT hardware qualification. It never
accesses an ESC or creates a flashable image. v0.9.1 is a diagnostic-only
source; all motor-output configurations must remain compile-forbidden.
"""
import argparse
import pathlib
import re
import sys

root = pathlib.Path(__file__).resolve().parents[2]
src = (root / "v0.9.1" / "README.md").read_text() if (root / "v0.9.1" / "README.md").exists() else ""
p = argparse.ArgumentParser()
p.add_argument("--source", type=pathlib.Path, help="path to full reconstructed src/main.c", required=True)
p.add_argument("--purpose", choices=["diagnostic", "motor"], default="diagnostic")
args = p.parse_args()
code = args.source.read_text()
rules = {
 "motor arming compile-forbidden":
     bool(re.search(r"#if\\s+POWER_STAGE_ARM_ALLOWED\\s*\\|\\|\\s*SENSORLESS_RUN_ALLOWED\\s*\\n#error",code)),
 "no claimed hardware gate validity":
     bool(re.search(r"#define\\s+COMM_GATE_HW_VALID\\s+0u",code)),
 "no claimed calibrated current scale":
     bool(re.search(r"#define\\s+COMM_CURRENT_SCALE_HW_VALID\\s+0u",code)),
 "no claimed PWM-coherent current samples":
     bool(re.search(r"#define\\s+COMM_ADC_TIMING_HW_VALID\\s+0u",code)),
 "no PWM half-bridge channel enable in TIM1 initialisation":
     "TIM_CCER(TIM1_BASE)=ADC_CCER_MASK;" in code,
 "PC14 button only after board-specific DRV126 comparison":
     "#define POWER_BUTTON_GPIO_BASE GPIOC_BASE" in code and "#define POWER_BUTTON_PIN 14u" in code,
 "PB1 not used as fictitious gate-enable":
     "gate_enable_gpio_init" not in code and not bool(re.search(r"gpio_cfg_nibble\\(GPIOB_BASE\\s*,\\s*1\\s*,",code)),
}
for name, passed in rules.items():
    print(("PASS" if passed else "FAIL")+": "+name)
if not all(rules.values()):
    sys.exit("FAIL: even diagnostic source has lost one or more guardrails.")
if args.purpose == "motor":
    print("BLOCKED: MT8006A HIN/LIN wiring, gate polarity, break/overcurrent hardware,")
    print("phase-current ADC gain/timing, and 10S/60V limits lack physical proof.")
    print("Compilation success and DRV126 boot do not authorize a motor release.")
    sys.exit(2)
print("PASS: diagnostic-only software guard. NOT a safe-to-flash approval.")
