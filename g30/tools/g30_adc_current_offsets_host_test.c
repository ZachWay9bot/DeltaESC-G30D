#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "g30_adc_current_offsets.h"
#include "stock_current_frontend.h"
int main(void) {
    uint16_t out=0u;
    const uint16_t a1[3]={2040u,2070u,2080u};
    const uint16_t a2ch4=2090u,a2ch5=2010u;
    assert(g30_adc_offset_stock_zero_valid(2048u));
    assert(g30_adc_offset_stock_zero_valid(1948u));
    assert(g30_adc_offset_stock_zero_valid(2148u));
    assert(!g30_adc_offset_stock_zero_valid(1947u));
    assert(!g30_adc_offset_stock_zero_valid(2149u));
    for(uint8_t sector=1u;sector<=6u;sector++) {
        const uint8_t uses_ch4=(uint8_t)(sector==4u || sector==5u);
        /* ADC2 sees +30 counts, ADC1 reference must see exactly +30. */
        uint16_t raw=(uint16_t)((uses_ch4?a2ch4:a2ch5)+30u);
        assert(g30_adc2_current_offset_normalize(sector,raw,a1,a2ch4,a2ch5,1u,1u,&out));
        assert(out==(uint16_t)((uses_ch4?a1[1]:a1[2])+30u));
        /* Each quadrant requires calibration of the ADC2 channel ACTUALLY used. */
        assert(g30_adc2_current_offset_normalize(sector,raw,a1,a2ch4,a2ch5,0u,1u,&out)==!uses_ch4);
        assert(g30_adc2_current_offset_normalize(sector,raw,a1,a2ch4,a2ch5,1u,0u,&out)==uses_ch4);
        const stock_adc_pair_t pair=stock_current_pair_for_sector(sector);
        assert(pair.adc2_channel==(uses_ch4?4u:5u));
        /* Both ADCs at zero => all three reconstructed phase currents zero. */
        const uint16_t rawzero=(uint16_t)(uses_ch4?a2ch4:a2ch5);
        assert(g30_adc2_current_offset_normalize(sector,rawzero,a1,a2ch4,a2ch5,1u,1u,&out));
        const uint16_t raw1=a1[pair.adc1_channel-3u];
        const stock_phase_currents_t z=stock_current_reconstruct_counts(sector,raw1,out,a1);
        assert(z.ia==0 && z.ib==0 && z.ic==0);
        /* Across 201 legal signed deltas, using either ADC2 physical zero
         * reproduces precisely the same signed current as ADC1's reference. */
        for(int16_t delta=-100;delta<=100;delta++) {
            const uint16_t raw_pair=(uint16_t)((uses_ch4?a2ch4:a2ch5)+delta);
            assert(g30_adc2_current_offset_normalize(sector,raw_pair,a1,
                a2ch4,a2ch5,1u,1u,&out));
            assert((int32_t)out-(int32_t)a1[pair.adc2_channel-3u]==delta);
            const stock_phase_currents_t phase=stock_current_reconstruct_counts(sector,raw1,out,a1);
            assert((int32_t)phase.ia+phase.ib+phase.ic==0);
        }
    }
    assert(!g30_adc2_current_offset_normalize(0,2048,a1,a2ch4,a2ch5,1,1,&out));
    assert(!g30_adc2_current_offset_normalize(7,2048,a1,a2ch4,a2ch5,1,1,&out));
    assert(!g30_adc2_current_offset_normalize(1,4095,a1,a2ch4,a2ch5,1,1,&out));
    assert(!g30_adc2_current_offset_normalize(4,0,a1,a2ch4,a2ch5,1,1,&out));
    assert(!g30_adc2_current_offset_normalize(1,2048,0,a2ch4,a2ch5,1,1,&out));
    assert(!g30_adc2_current_offset_normalize(1,2048,a1,a2ch4,0,1,1,&out));
    assert(!g30_adc2_current_offset_normalize(1,2048,a1,a2ch4,a2ch5,1,1,0));
    puts("PASS ADC1/ADC2 separate CH4+CH5 offsets; zero-current all sectors; invalid fail closed");
    return 0;
}
