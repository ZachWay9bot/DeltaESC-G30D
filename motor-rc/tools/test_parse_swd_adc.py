#!/usr/bin/env python3
from pathlib import Path
import sys
sys.path.insert(0,str(Path(__file__).resolve().parent))
from parse_swd_adc import regular_channels,injected_channels,input_analog,parse_cli_log,audit
def test():
    assert injected_channels(3<<15)==[3]
    assert injected_channels((1<<20)|(4<<10)|(5<<15))==[4,5]
    assert regular_channels(0,0,3|(4<<5)|(5<<10))==[3]
    assert regular_channels((2<<20),0,3|(4<<5)|(5<<10))==[3,4,5]
    assert input_analog(0,1) and not input_analog(0x4<<4,1)
    regs={"ADC1_SQR1":2<<20,"ADC1_SQR2":0,"ADC1_SQR3":3|(4<<5)|(5<<10),
          "ADC1_JSQR":3<<15,
          "ADC2_SQR1":0,"ADC2_SQR2":0,"ADC2_SQR3":1,
          "ADC2_JSQR":5<<15, "GPIOA_CRL":0,"GPIOB_CRL":0x4<<4}
    r=audit(regs)
    assert r["complete"]
    assert r["ADC1"]["regular_pins"]==["PA3","PA4","PA5"]
    assert r["ADC1"]["injected_pins"]==["PA3"]
    assert r["ADC2"]["injected_pins"]==["PA5"]
    assert r["signal_type"]=="UNVERIFIED"
    assert not r["GPIOB_CRL"]["PB1_analog"]
    assert audit({"ADC1_JSQR":0})["complete"] is False
    log="\n".join(f"### {k} 0x{__import__('parse_swd_adc').ADDR[k]:08X}\n0x{__import__('parse_swd_adc').ADDR[k]:08X} : 0x{v:08X}" for k,v in regs.items())
    assert parse_cli_log(log)==regs
    # A row from a DIFFERENT register must never count for the current one.
    bad="### ADC1_JSQR 0x40012438\n0x4001243C : 0xFFFFFFFF\n"
    assert parse_cli_log(bad)=={}
    print("PASS read-only SWD ADC channel decode and ambiguity handling: 12 cases")
if __name__=="__main__":test()
