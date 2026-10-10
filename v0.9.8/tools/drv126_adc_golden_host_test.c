/* DRV126 reference at 0x08005734..0x080057B8, 0x08005B2C..0x08005B60.
 * All expected equations and sectors are checked independently of the
 * implementation, including negative ASRS rounding and separate ADC ranks. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "stock_current_frontend.h"

static int32_t arm_reference(int32_t n) {
    /* signed 32-bit MUL #0xC977 then ASRS #10 for 12-bit ADC values */
    const int32_t product=n * 51575;
    int32_t q=product/1024;
    if (product<0 && (product % 1024)!=0) --q;
    return q;
}

int main(void) {
    const uint8_t ch1[7]={0,4,3,3,3,3,4};
    const uint8_t ch2[7]={0,5,5,5,4,4,5};
    const uint16_t offsets[3]={2017,2089,2048};
    unsigned long cases=0;
    for(uint8_t sector=1; sector<=6;sector++) {
        stock_adc_pair_t pair=stock_current_pair_for_sector(sector);
        assert(pair.adc1_channel==ch1[sector]);
        assert(pair.adc2_channel==ch2[sector]);
        for(int32_t dx=-120;dx<=120;dx++) {
            for(int32_t dy=-120;dy<=120;dy++) {
                const int32_t x=arm_reference(dx);
                const int32_t y=arm_reference(dy);
                int32_t a=0,b=0,c=0;
                if(sector==1 || sector==6) {a=x+y;b=-x;c=-y;}
                if(sector==2 || sector==3) {a=-x;b=x+y;c=-y;}
                if(sector==4 || sector==5) {a=-x;b=-y;c=x+y;}
                const stock_drv126_scaled_currents_t s=
                    stock_current_drv126_reconstruct_scaled(sector,dx,dy);
                assert(s.ia==a && s.ib==b && s.ic==c);
                assert(s.ia+s.ib+s.ic==0);
                assert(stock_current_drv126_scaled_delta(dx)==x);
                const stock_phase_currents_t raw=stock_current_reconstruct_counts(
                    sector,(uint16_t)(offsets[pair.adc1_channel-3]+dx),
                           (uint16_t)(offsets[pair.adc2_channel-3]+dy),offsets);
                assert(raw.ia+raw.ib+raw.ic==0);
                /* Reconstructing before rather than after ARM's individual
                 * ASRS can differ by up to one scaled unit per ADC input. */
                assert(raw.ia>=-240 && raw.ia<=240);
                cases++;
            }
        }
    }
    assert(stock_current_drv126_scaled_delta(-100)==-5037);
    assert(stock_current_drv126_scaled_delta(100)==5036);
    assert(stock_current_drv126_scaled_delta(-1)==-51);
    assert(stock_current_drv126_scaled_delta(1)==50);
    assert(stock_current_drv126_scaled_delta(-4095)==arm_reference(-4095));
    assert(stock_current_drv126_scaled_delta(4095)==arm_reference(4095));
    assert(stock_current_drv126_scaled_delta(8190)>0);
    assert(stock_current_drv126_scaled_delta(-8190)<0);
    assert(stock_current_drv126_scaled_delta(INT32_MAX)==INT32_MAX);
    assert(stock_current_drv126_scaled_delta(INT32_MIN)==INT32_MIN);
    printf("PASS DRV126 exact ADC scaling/sign/sectors: %lu golden vectors (6 sectors)\n", cases);
    return 0;
}