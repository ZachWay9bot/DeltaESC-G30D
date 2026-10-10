#pragma once
#include <stdint.h>
/* ADC2 CH4 has its OWN offset. ADC1 CH4 zero cannot be used as ADC2 CH4
 * zero; equal pin number does not imply equal ADC path offset. */
uint8_t g30_adc2_ch4_offset_normalize(uint8_t sector,
    uint16_t adc2_raw, uint16_t adc1_ch4_offset,
    uint16_t adc2_ch4_offset, uint8_t adc2_ch4_calibrated,
    uint16_t *normalized);