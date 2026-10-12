#!/usr/bin/env python3
"""v0.9.20 source contract: preserve the sensorless target while hardware drive stays locked."""
from pathlib import Path
import sys

root=Path(sys.argv[1] if len(sys.argv)>1 else ".").resolve()
def read(rel):
    p=root/rel
    if not p.is_file():
        raise SystemExit(f"FAIL missing {rel}")
    return p.read_text(errors="strict")

main=read("src/main.c")
sl=read("src/sensorless_control.c")
obs=read("src/vesc_ebics_flux.c")
auto=read("src/motor_id_auto.c")
mk=read("Makefile")
tests=read("tools/run_v114_tests.sh")

checks=[
    ("VESC/EBiCS observer compile switch", "VESC_EBICS_OBSERVER" in sl),
    ("Ortega observer call", "vesc_ebics_flux_step" in sl),
    ("observer source equation", "VESC Ortega observer" in obs or "VESC Ortega flux-observer" in obs),
    ("sensorless state machine handover", "SENSORLESS_HANDOVER" in sl and "SENSORLESS_CLOSED_LOOP" in sl),
    ("Motor-ID stages sensorless control", "sensorless_control_t staged_control" in auto),
    ("Motor-ID commits into staged sensorless control", "motor_id_commit(&a->id,&staged_control)" in auto),
    ("Motor-ID publishes staged sensorless control", "copy_struct(a->control,&staged_control" in auto),
    ("sensorless PMSM regression retained", "sensorless_plant_host_test.c" in tests),
    ("ADC->FOC->TIM1 pipeline regression retained", "motor_pipeline_host_test.c" in tests),
    ("observer regression retained", "vesc_ebics_flux_host_test.c" in tests),
    ("sensorless source linked", "src/sensorless_control.c" in mk and "src/vesc_ebics_flux.c" in mk),
    ("normal sensorless drive remains compile-locked",
     'Motor RC permits E7-only Motor-ID, never drive or sensorless E5' in main),
    ("current scale remains unqualified", "#define COMM_CURRENT_SCALE_HW_VALID 0u" in main),
    ("ADC timing remains unqualified", "#define COMM_ADC_TIMING_HW_VALID 0u" in main),
]
for label,ok in checks:
    if not ok:
        raise SystemExit("FAIL "+label)
    print("PASS "+label)

# Do not let a Hall fallback silently enter the actual sensorless control core.
for rel,src in [("src/sensorless_control.c",sl),("src/vesc_ebics_flux.c",obs)]:
    if "hall" in src.lower():
        raise SystemExit(f"FAIL Hall dependency found in {rel}")
print("PASS no Hall dependency in sensorless observer/control core")
print("PASS v0.9.20 sensorless continuity contract")
