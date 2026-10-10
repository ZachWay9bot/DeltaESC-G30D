#include "g30_adc_current_offsets.h"
uint8_t g30_adc2_ch4_offset_normalize(uint8_t sector,
    uint16_t adc2_raw, uint16_t adc1_ch4_offset,
    uint16_t adc2_ch4_offset, uint8_t adc2_ch4_calibrated,
    uint16_t *normalized) {
    if (!normalized || adc2_raw>4095u || adc1_ch4_offset>4095u ||
        adc2_ch4_offset>4095u || sector<1u || sector>6u)
        return 0u;
    if (sector!=4u && sector!=5u) {
        *normalized=adc2_raw;
        return 1u;
    }
    if (!adc2_ch4_calibrated) return 0u;
    const int32_t result=(int32_t)adc2_raw+
        (int32_t)adc1_ch4_offset-(int32_t)adc2_ch4_offset;
    if (result<0 || result>4095) return 0u;
    *normalized=(uint16_t)result;
    return 1u;
}