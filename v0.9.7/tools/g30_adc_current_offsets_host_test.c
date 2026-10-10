#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "g30_adc_current_offsets.h"
int main(void) {
    uint16_t out=0u;
    for(uint8_t sector=1u;sector<=6u;sector++) {
        assert(g30_adc2_ch4_offset_normalize(sector,2130u,2040u,2070u,1u,&out));
        assert(out==((sector==4u||sector==5u)?2100u:2130u));
        assert(g30_adc2_ch4_offset_normalize(sector,2130u,2040u,2070u,0u,&out)==
               (uint8_t)((sector!=4u&&sector!=5u)?1u:0u));
    }
    assert(!g30_adc2_ch4_offset_normalize(4u,4090u,4095u,2000u,1u,&out));
    assert(!g30_adc2_ch4_offset_normalize(4u,1u,0u,200u,1u,&out));
    assert(!g30_adc2_ch4_offset_normalize(0u,2000u,2100u,2100u,1u,&out));
    assert(!g30_adc2_ch4_offset_normalize(4u,2000u,2100u,2100u,1u,0));
    puts("PASS ADC2 CH4 independent offset and 6-sector bias normalization");
    return 0;
}