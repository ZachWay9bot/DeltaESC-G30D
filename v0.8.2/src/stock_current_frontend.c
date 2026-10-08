#include "stock_current_frontend.h"

#define DRV126_CURRENT_SCALE_MUL 51575
#define DRV126_CURRENT_SCALE_SHIFT 10

uint32_t stock_current_jsqr_one(uint8_t channel) {
    return ((uint32_t)(channel & 0x1fu)) << 15;
}

stock_adc_pair_t stock_current_pair_for_sector(uint8_t sector) {
    stock_adc_pair_t p={3u,5u};
    switch (sector) {
    case 1u: case 6u: p.adc1_channel=4u; p.adc2_channel=5u; break;
    case 2u: case 3u: p.adc1_channel=3u; p.adc2_channel=5u; break;
    case 4u: case 5u: p.adc1_channel=3u; p.adc2_channel=4u; break;
    default: break;
    }
    return p;
}

uint8_t stock_current_sector_from_ab(int32_t a, int32_t b) {
    int32_t sqrt3_a=(a * 1774) >> 10;
    int32_t u=(b + sqrt3_a) >> 1;
    int32_t v=(b - sqrt3_a) >> 1;
    if (u < 0) {
        if (v < 0) return 5u;
        if (b <= 0) return 4u;
        return 3u;
    }
    if (v >= 0) return 2u;
    if (b <= 0) return 6u;
    return 1u;
}

stock_phase_currents_t stock_current_reconstruct_counts(uint8_t sector,
                                                        uint16_t adc1_raw,
                                                        uint16_t adc2_raw,
                                                        const uint16_t o[3]) {
    stock_phase_currents_t r={0,0,0};
    int32_t x=0,y=0;
    switch (sector) {
    case 1u: case 6u:
        x=(int32_t)adc1_raw-(int32_t)o[1];
        y=(int32_t)adc2_raw-(int32_t)o[2];
        r.ia=(int16_t)(x+y); r.ib=(int16_t)(-x); r.ic=(int16_t)(-y); break;
    case 2u: case 3u:
        x=(int32_t)adc1_raw-(int32_t)o[0];
        y=(int32_t)adc2_raw-(int32_t)o[2];
        r.ia=(int16_t)(-x); r.ib=(int16_t)(x+y); r.ic=(int16_t)(-y); break;
    case 4u: case 5u:
        x=(int32_t)adc1_raw-(int32_t)o[0];
        y=(int32_t)adc2_raw-(int32_t)o[1];
        r.ia=(int16_t)(-x); r.ib=(int16_t)(-y); r.ic=(int16_t)(x+y); break;
    default: break;
    }
    return r;
}

int32_t stock_current_scaled_unit_from_delta(int32_t adc_delta) {
    return (adc_delta * DRV126_CURRENT_SCALE_MUL) >> DRV126_CURRENT_SCALE_SHIFT;
}
