#pragma once
#include <stdint.h>

/* STM32F103/RM0008, G30D 16 kHz centre-aligned PWM profile.
 * ADC1+ADC2 dual-injected, one rank per ADC, TIM1-CC4 common trigger.
 * This is a DIGITAL VALIDITY CHECK, not an analogue calibration certificate. */
typedef struct {
    uint32_t rcc_cfgr;
    uint32_t adc1_cr1, adc2_cr1;
    uint32_t adc1_cr2, adc2_cr2;
    uint32_t adc1_smpr2, adc2_smpr2;
    uint16_t tim1_cr1, tim1_ccer;
    uint16_t tim1_arr, tim1_ccr4;
    uint16_t tim1_ccr1, tim1_ccr2, tim1_ccr3;
    uint8_t sector;
} g30_adc_current_state_t;

enum {
    G30_ADC_F_CLOCK = 1u<<0,
    G30_ADC_F_DUAL = 1u<<1,
    G30_ADC_F_TRIGGER = 1u<<2,
    G30_ADC_F_SAMPLE = 1u<<3,
    G30_ADC_F_TIM1 = 1u<<4,
    G30_ADC_F_WINDOW = 1u<<5,
    G30_ADC_F_SECTOR = 1u<<6
};
/* Zero = internally consistent register and conservative sample aperture.
 * Nonzero = refuse current feedback for this PWM sector. */
uint32_t g30_adc_current_flags(const g30_adc_current_state_t *s);