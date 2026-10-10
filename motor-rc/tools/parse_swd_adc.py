#!/usr/bin/env python3
"""Decode READ-ONLY STM32F103 ADC sequencer snapshots from the SWD HOTPLUG audit.

No MCU write commands. No automatic BEMF channel assumptions.
STM32F103 RM0008 ADC regular SQR1/2/3 and injected JSQR mapping.
"""
from __future__ import annotations
import argparse
import json
import re
from pathlib import Path

ADDR = {
    "ADC1_SQR1": 0x4001242C, "ADC1_SQR2": 0x40012430,
    "ADC1_SQR3": 0x40012434, "ADC1_JSQR": 0x40012438,
    "ADC2_SQR1": 0x4001282C, "ADC2_SQR2": 0x40012830,
    "ADC2_SQR3": 0x40012834, "ADC2_JSQR": 0x40012838,
    "ADC1_CR1": 0x40012404, "ADC1_CR2": 0x40012408,
    "ADC2_CR1": 0x40012804, "ADC2_CR2": 0x40012808,
    "GPIOA_CRL": 0x40010800, "GPIOB_CRL": 0x40010C00,
    "TIM1_CCER": 0x40012C20, "TIM1_BDTR": 0x40012C44,
}
ADC_PINS = {
    0:"PA0",1:"PA1",2:"PA2",3:"PA3",4:"PA4",5:"PA5",
    6:"PA6",7:"PA7",8:"PB0",9:"PB1",10:"PC0",11:"PC1",
    12:"PC2",13:"PC3",14:"PC4",15:"PC5",16:"TEMP",17:"VREF",
}

def regular_channels(sqr1: int, sqr2: int, sqr3: int) -> list[int]:
    size = ((sqr1 >> 20) & 15) + 1
    return [((sqr3 if i < 6 else sqr2 if i < 12 else sqr1)
             >> ((i % 6) * 5)) & 31 for i in range(size)]

def injected_channels(jsqr: int) -> list[int]:
    n = ((jsqr >> 20) & 3) + 1
    # RM0008: single injected sample lives in JSQ4, not JSQ1.
    return [(jsqr >> (5 * (4-n+i))) & 31 for i in range(n)]

def input_analog(gpio_crl: int, bit: int) -> bool:
    # GPIO_CRL pin nibble 0000 (MODE=00 CNF=00) => analog input
    return ((gpio_crl >> (4 * bit)) & 15) == 0

def parse_cli_log(log: str) -> dict[str,int]:
    found: dict[str,int] = {}
    current = None
    for line in log.splitlines():
        header = re.match(r"^###\s+([A-Z][A-Z0-9_]+)\s+(0x[0-9A-Fa-f]{8})\s*$", line)
        if header:
            name, address = header.groups()
            current = name if ADDR.get(name)==int(address,16) else None
            continue
        if not current or current in found:
            continue
        # STM32_Programmer_CLI memory read output usually:
        # '0x4001242C : 0x00100000' (some versions omit value prefix).
        match = re.search(r"(0x[0-9A-Fa-f]{8})\s*:\s*(?:0x)?([0-9A-Fa-f]{8})\b",line)
        if not match:
            continue
        address,value=match.groups()
        if int(address,16)==ADDR[current]:
            found[current]=int(value,16)
    return found

def audit(regs: dict[str,int]) -> dict:
    if not all(x in regs for x in ("ADC1_SQR1","ADC1_SQR2","ADC1_SQR3",
                                  "ADC2_SQR1","ADC2_SQR2","ADC2_SQR3",
                                  "ADC1_JSQR","ADC2_JSQR")):
        return {"complete":False,"signal_type":"UNVERIFIED",
                "reason":"ADC register capture incomplete; do not infer BEMF channel mapping",
                "observed_registers":sorted(regs)}
    outputs={"complete":True,"signal_type":"UNVERIFIED"}
    for adc in ("ADC1","ADC2"):
        cr2=regs.get(f"{adc}_CR2")
        # When ADON=0 a zeroed SQR/JSQR is NOT a measurement on CH0.
        # Unknown CR2 means sequencing may be described but never "running".
        running=(cr2 is not None and bool(cr2 & 1))
        if running:
            regular=regular_channels(*(regs[f"{adc}_{tag}"] for tag in
                                       ("SQR1","SQR2","SQR3")))
            injected=injected_channels(regs[f"{adc}_JSQR"])
        else:
            regular=[]
            injected=[]
        outputs[adc]={"enabled_at_capture":running,
                      "cr2_present":cr2 is not None,
                      "regular_channels":regular,"regular_pins":
                      [ADC_PINS.get(c,"UNKNOWN") for c in regular],
                      "injected_channels":injected,"injected_pins":
                      [ADC_PINS.get(c,"UNKNOWN") for c in injected]}
    sampled=set(outputs["ADC1"]["regular_channels"]+
                outputs["ADC1"]["injected_channels"]+
                outputs["ADC2"]["regular_channels"]+
                outputs["ADC2"]["injected_channels"])
    outputs["PB1_ADC9_actually_selected"]=9 in sampled
    if "TIM1_CCER" in regs:
        outputs["TIM1_motor_ccer_channels_active"]=(regs["TIM1_CCER"] & 0x0555)!=0
    if "TIM1_BDTR" in regs:
        outputs["TIM1_BDTR_MOE"]=(regs["TIM1_BDTR"] & 0x8000)!=0
        outputs["TIM1_BDTR_AOE"]=(regs["TIM1_BDTR"] & 0x4000)!=0
    if "GPIOA_CRL" in regs:
        outputs["GPIOA_CRL"]={
            "PA3_analog":input_analog(regs["GPIOA_CRL"],3),
            "PA4_analog":input_analog(regs["GPIOA_CRL"],4),
            "PA5_analog":input_analog(regs["GPIOA_CRL"],5),
            "PA6_analog":input_analog(regs["GPIOA_CRL"],6),
            "PA7_analog":input_analog(regs["GPIOA_CRL"],7)}
    if "GPIOB_CRL" in regs:
        outputs["GPIOB_CRL"]={
            "PB0_analog":input_analog(regs["GPIOB_CRL"],0),
            "PB1_analog":input_analog(regs["GPIOB_CRL"],1)}
    outputs["conclusion"]=(
        "The sequencer reports which ADC inputs firmware currently samples. "
        "It CANNOT determine whether those inputs connect to actual motor phase "
        "voltage dividers. PA3/4/5 are already documented as shunt currents, "
        "PA1 as DC bus; PB1 descriptions conflict. "
        "NO passive-rotor-angle or sensorless lock assertion is allowed."
    )
    return outputs

def main():
    ap=argparse.ArgumentParser()
    ap.add_argument("log",type=Path)
    ap.add_argument("--json",action="store_true")
    args=ap.parse_args()
    result=audit(parse_cli_log(args.log.read_text(errors="replace")))
    if args.json: print(json.dumps(result,indent=2))
    else:
        print(json.dumps(result,indent=2,ensure_ascii=False))
        print("\nNicht flashen, solange keine realen BEMF-Phaseneingaenge belegt sind.")
    return 0 if result["complete"] else 2

if __name__=="__main__":
    raise SystemExit(main())
