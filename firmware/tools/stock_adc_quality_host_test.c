#include <stdint.h>
#include <stdio.h>
#include "stock_adc_quality.h"
#include "stock_current_frontend.h"
#define CHECK(expr) do{if(!(expr)){printf("FAIL line %d: %s\n",__LINE__,#expr);return 1;}}while(0)
int main(void) {
    stock_adc_quality_t q={0};
    for(uint8_t sec=1u;sec<=6u;sec++) {
        const stock_adc_pair_t p=stock_current_pair_for_sector(sec);
        CHECK(stock_adc_quality_check(&q,sec,2000u,2050u,
            stock_current_jsqr_one(p.adc1_channel),stock_current_jsqr_one(p.adc2_channel),
            4000u,4000u)==0u);
    }
    CHECK(q.total_checked==6u && q.fault_latched==0u);
    const stock_adc_pair_t p=stock_current_pair_for_sector(1u);
    CHECK((stock_adc_quality_check(&q,1u,4096u,2050u,
        stock_current_jsqr_one(p.adc1_channel),stock_current_jsqr_one(p.adc2_channel),
        4000u,4000u)&ADCQ_RAW_INVALID)!=0u);
    CHECK(q.invalid_raw_events==1u && q.fault_latched==1u);
    CHECK(stock_adc_quality_check(&q,1u,2048u,2050u,
        stock_current_jsqr_one(3u),stock_current_jsqr_one(p.adc2_channel),
        4000u,4000u)==ADCQ_PAIR_MISMATCH);
    CHECK(q.wrong_channel_events==1u);
    CHECK(stock_adc_quality_check(&q,0u,2048u,2050u,0u,0u,0u,4000u)==ADCQ_PAIR_MISMATCH);
    CHECK(stock_adc_quality_check(&q,1u,2048u,2050u,
        stock_current_jsqr_one(p.adc1_channel),stock_current_jsqr_one(p.adc2_channel),
        9000u,4000u)==ADCQ_GAP_LONG);
    CHECK(stock_adc_quality_check(&q,1u,2048u,2050u,
        stock_current_jsqr_one(p.adc1_channel),stock_current_jsqr_one(p.adc2_channel),
        1200u,4000u)==ADCQ_INTERVAL_SHORT);
    CHECK(q.long_gap_events==1u && q.short_gap_events==1u);
    CHECK(stock_adc_quality_check(0u,1u,2048u,2050u,
        stock_current_jsqr_one(p.adc1_channel),stock_current_jsqr_one(p.adc2_channel),
        4000u,4000u)==ADCQ_BAD_PAIR);
    q.invalid_raw_events=65535u;
    (void)stock_adc_quality_check(&q,1u,0xffffu,0u,
        stock_current_jsqr_one(p.adc1_channel),stock_current_jsqr_one(p.adc2_channel),4000u,4000u);
    CHECK(q.invalid_raw_events==65535u);
    puts("PASS ADC quality: six sectors, register mapping, invalid 12-bit samples, timing counters, saturation");
    return 0;
}
