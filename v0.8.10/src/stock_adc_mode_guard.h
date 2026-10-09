#pragma once
#include <stdint.h>

/* STM32F103 RM0008: ADC1_CR1.DUALMOD=0101 is injected simultaneous ONLY.
 * Combined REGULAR+injected simultaneous 0001 cannot isolate ADC1 PA1 VBUS. */
#define G30_ADC_DUALMOD_MASK (0xFu << 16)
#define G30_ADC_DUALMOD_INJECTED_ONLY (5u << 16)
#define G30_ADC_JEOC (1u << 2)
#define G30_ADCMODE_BAD_MASTER 0x01u
#define G30_ADCMODE_BAD_SLAVE  0x02u
#define G30_ADCMODE_ADC2_NOT_READY 0x04u

typedef struct {
    uint32_t adc1_cr1, adc2_cr1, adc1_cr2;
    uint16_t adc2_sr;
    uint8_t last_flags, ever_faulted;
    uint16_t fault_count;
} g30_adc_mode_guard_t;

uint8_t g30_adc_mode_flags(uint32_t adc1_cr1,uint32_t adc2_cr1,uint32_t adc2_sr);
uint8_t g30_adc_mode_capture(g30_adc_mode_guard_t *g,uint32_t adc1_cr1,
                             uint32_t adc2_cr1,uint32_t adc1_cr2,uint32_t adc2_sr);
void g30_adc_mode_encode_ee(const g30_adc_mode_guard_t *g,uint8_t out[16]);
