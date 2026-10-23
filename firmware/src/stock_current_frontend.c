#include "stock_current_frontend.h"
#include <limits.h>

#define G30_NOMINAL_MA_NUM 51575LL
#define G30_NOMINAL_MA_SHIFT 10

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

int32_t stock_current_drv126_scaled_delta(int32_t adc_delta) {
    /* Original DRV126 at 0x08005766, 0x08005786, 0x080057A4:
     * signed 32-bit MUL #0xC977 followed by ASRS #10. For normal injected
     * ADC deltas [-4095,4095] signed multiplication fits 32 bits.
     * Avoid implementation-dependent C right-shift of negative values. */
    const int64_t product=(int64_t)adc_delta * 51575;
    const int64_t result=product>=0 ? product / 1024 :
        -((-product+1023) / 1024);
    /* Single ADC deltas [-4095,4095] are identical to stock ARM MUL+ASRS.
     * Aggregated reconstructed phase counts can span [-8190,8190], so
     * overflow/zero-on-overrange behavior would silently defeat protection. */
    if(result>INT32_MAX) return INT32_MAX;
    if(result<INT32_MIN) return INT32_MIN;
    return (int32_t)result;
}

stock_drv126_scaled_currents_t stock_current_drv126_reconstruct_scaled(
    uint8_t sector, int32_t adc1_delta, int32_t adc2_delta) {
    stock_drv126_scaled_currents_t r={0,0,0};
    const int32_t x=stock_current_drv126_scaled_delta(adc1_delta);
    const int32_t y=stock_current_drv126_scaled_delta(adc2_delta);
    switch(sector) {
    case 1u: case 6u: r.ia=x+y; r.ib=-x; r.ic=-y; break;
    case 2u: case 3u: r.ia=-x; r.ib=x+y; r.ic=-y; break;
    case 4u: case 5u: r.ia=-x; r.ib=-y; r.ic=x+y; break;
    default: break;
    }
    return r;
}

int32_t stock_current_nominal_counts_to_ma(int32_t counts) {
    /* DRV126 numerical fixed-point transform, used as nominal physical mA.
     * Analogue shunt+gain scaling on the modified ESC is not certified. */
    return stock_current_drv126_scaled_delta(counts);
}
int16_t stock_current_nominal_ma_to_counts(uint16_t ma) {
    int32_t counts = (int32_t)(((uint32_t)ma << G30_NOMINAL_MA_SHIFT) / 51575u);
    if (counts < 1) counts = 1;
    if (counts > 32767) counts = 32767;
    return (int16_t)counts;
}
int32_t stock_current_scaled_unit_from_delta(int32_t adc_delta) {
    return stock_current_drv126_scaled_delta(adc_delta);
}
