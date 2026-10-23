#pragma once
#include <stdint.h>
/* The ADC1 and ADC2 input amplifiers have independent zero-current offsets.
 * ADC2 CH5 is used in sectors 1/2/3/6 and CH4 in sectors 4/5.
 * Convert the ADC2 raw value onto the ADC1-based legacy reconstruction
 * coordinate system. Never silently substitute an ADC1 offset for ADC2. */
uint8_t g30_adc2_current_offset_normalize(uint8_t sector,
    uint16_t adc2_raw, const uint16_t adc1_offsets[3],
    uint16_t adc2_ch4_offset, uint16_t adc2_ch5_offset,
    uint8_t adc2_ch4_valid, uint8_t adc2_ch5_valid,
    uint16_t *normalized);
/* DRV126 stock boot offset acceptance: within 100 counts of 2048. */
uint8_t g30_adc_offset_stock_zero_valid(uint16_t offset);
