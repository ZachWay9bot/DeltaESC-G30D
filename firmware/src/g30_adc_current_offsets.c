#include "g30_adc_current_offsets.h"

uint8_t g30_adc_offset_stock_zero_valid(uint16_t offset) {
    return (uint8_t)(offset>=1948u && offset<=2148u);
}

uint8_t g30_adc2_current_offset_normalize(uint8_t sector,
    uint16_t raw, const uint16_t adc1_offsets[3],
    uint16_t adc2_ch4_offset, uint16_t adc2_ch5_offset,
    uint8_t valid4, uint8_t valid5, uint16_t *normalized) {
    if (!normalized || !adc1_offsets || sector<1u || sector>6u || raw>4095u)
        return 0u;
    uint16_t adc1_ref=0u, adc2_ref=0u;
    if (sector==4u || sector==5u) {
        if (!valid4) return 0u;
        adc1_ref=adc1_offsets[1]; adc2_ref=adc2_ch4_offset;
    } else {
        if (!valid5) return 0u;
        adc1_ref=adc1_offsets[2]; adc2_ref=adc2_ch5_offset;
    }
    if (!g30_adc_offset_stock_zero_valid(adc1_ref) ||
        !g30_adc_offset_stock_zero_valid(adc2_ref)) return 0u;
    const int32_t v=(int32_t)raw+(int32_t)adc1_ref-(int32_t)adc2_ref;
    if (v<0 || v>4095) return 0u;
    *normalized=(uint16_t)v;
    return 1u;
}
