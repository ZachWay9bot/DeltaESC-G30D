#!/usr/bin/env python3
"""DeltaESC v0.9.1 static regression checks. Never connects to a controller."""
from pathlib import Path
import re

root = Path(__file__).resolve().parents[1]
s = (root / 'src/main.c').read_text()
h = (root / 'src/power_logic.h').read_text()
logic = (root / 'src/power_logic.c').read_text()

checks = {
    'PC14 declared power button': '#define POWER_BUTTON_PIN 14u' in s and '#define POWER_BUTTON_GPIO_BASE GPIOC_BASE' in s,
    'PC14 configured only as input': 'gpio_cfg_nibble(POWER_BUTTON_GPIO_BASE, POWER_BUTTON_PIN, 0x4u)' in s,
    'PC14 active-high sampled': 'GPIO_IDR(POWER_BUTTON_GPIO_BASE)&(1u<<POWER_BUTTON_PIN))!=0u' in s,
    'PA11 latch configured before PLL': 'irq_disable();power_hold_init();clock_64mhz_hsi();' in s,
    'PA11 power latch': '#define POWER_HOLD_PIN 11u' in s and 'GPIO_BSRR(GPIOA_BASE) = (1u << POWER_HOLD_PIN);' in s,
    'PB1 never configured or written': not any(re.search(x,s) for x in [r'gpio_cfg_nibble\(GPIOB_BASE\s*,\s*1\s*,',r'GPIO_(?:BSRR|BRR)\(GPIOB_BASE\)\s*=\s*\(1u\s*<<\s*1\)',r'GPIO_ODR\(GPIOB_BASE\)\s*&\s*\(1u\s*<<\s*1\)']),
    'No PB1 gate control': 'gate_enable_gpio_init' not in s,
    'Powered builds require remaining ADC and current safeguards': '#if SENSORLESS_RUN_ALLOWED || POWER_STAGE_ARM_ALLOWED' in s and '#error "DRV126 gate profile ready' in s,
    'Gate channels disabled at init': 'TIM_CCER(TIM1_BASE)=stock_tim1_gate_ccer_off(STOCK_TIM1_GATE_RUN_CCER);' in s,
    'Stock gate route qualified, live current and timing still closed': ('#define COMM_GATE_STOCK_PROFILE_READY 1u' in s) and all((f'#define {x} 0u') in s for x in ['COMM_CURRENT_SCALE_HW_VALID','COMM_ADC_TIMING_HW_VALID']),
    'Power logic semantic name corrected': 'pressed_sample' in h and 'pressed_sample' in logic,
}
for label, passed in checks.items():
    print(f'{"PASS" if passed else "FAIL"}: {label}')
if not all(checks.values()):
    raise SystemExit(1)