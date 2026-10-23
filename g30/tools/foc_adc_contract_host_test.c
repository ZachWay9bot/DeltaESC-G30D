#include <stdio.h>
#include <stdint.h>
#include "foc_adc_contract.h"
#define REQUIRE(x) do { if (!(x)) { printf("FAIL line %d: %s\n",__LINE__,#x);return 1;} } while(0)
static const uint16_t off[3] = {2050u, 2020u, 2030u};
static foc_adc_sample_t sample(uint8_t sector,uint32_t seq) {
    stock_adc_pair_t p=stock_current_pair_for_sector(sector);
    foc_adc_sample_t s={0};
    s.sequence=seq;s.sector=sector;
    s.adc1_jsqr=stock_current_jsqr_one(p.adc1_channel);
    s.adc2_jsqr=stock_current_jsqr_one(p.adc2_channel);
    s.adc1_sr=4u;s.adc2_sr=4u;
    s.adc1_jdr1=(uint16_t)(off[p.adc1_channel-3u]+3u);
    s.adc2_jdr1=(uint16_t)(off[p.adc2_channel-3u]+5u);
    s.dual_injected_simultaneous=1u;
    s.sample_window_valid=1u;
    return s;
}
static int positive(void) {
    foc_adc_contract_t g={0};foc_adc_result_t out={0};
    foc_adc_contract_init(&g);
    for(uint8_t s=1;s<=6;s++) {
        uint32_t seq=100u+s;
        REQUIRE(foc_adc_contract_prepare(&g,s,seq,off,1));
        foc_adc_sample_t v=sample(s,seq);
        REQUIRE(foc_adc_contract_complete(&g,&v,&out));
        REQUIRE(out.valid && out.sector==s && out.sequence==seq);
        REQUIRE(out.counts.ia+out.counts.ib+out.counts.ic==0);
        REQUIRE(g.accepted==s && g.rejected==0u && !g.pending);
    }
    REQUIRE(!foc_adc_contract_reset(&g,0));
    REQUIRE(g.accepted==6u);
    REQUIRE(foc_adc_contract_reset(&g,1));
    REQUIRE(!g.seen_sample && !g.fault_latched);
    /* Sequence rollover 0xffffffff -> 0 is legal */
    REQUIRE(foc_adc_contract_prepare(&g,1,0xffffffffu,off,1));
    foc_adc_sample_t a=sample(1,0xffffffffu);
    REQUIRE(foc_adc_contract_complete(&g,&a,&out));
    REQUIRE(foc_adc_contract_prepare(&g,2,0u,off,1));
    a=sample(2,0u);
    REQUIRE(foc_adc_contract_complete(&g,&a,&out));
    return 0;
}
static int negative(void) {
    for (unsigned tc=0;tc<14;tc++) {
        foc_adc_contract_t g={0};foc_adc_result_t out={0};
        foc_adc_contract_init(&g);
        uint16_t broken_off[3]={2050u,2020u,5000u};
        if(tc==0) REQUIRE(!foc_adc_contract_prepare(&g,0,10u,off,1));
        else if(tc==1) REQUIRE(!foc_adc_contract_prepare(&g,7,10u,off,1));
        else if(tc==2) REQUIRE(!foc_adc_contract_prepare(&g,1,10u,off,0));
        else if(tc==3) REQUIRE(!foc_adc_contract_prepare(&g,1,10u,broken_off,1));
        else if(tc==4) REQUIRE(!foc_adc_contract_prepare(&g,1,10u,0,1));
        else {
            REQUIRE(foc_adc_contract_prepare(&g,1,10u,off,1));
            foc_adc_sample_t v=sample(1,10u);
            switch(tc) {
                case 5: REQUIRE(!foc_adc_contract_prepare(&g,1,10u,off,1));break;
                case 6: v.sector=2;break;
                case 7: v.sequence=11;break;
                case 8: v.adc1_jsqr ^= 1u<<20;break; /* two injected ranks */
                case 9: v.adc2_jsqr=stock_current_jsqr_one(3);break;
                case 10:v.adc2_sr=0;break;
                case 11:v.dual_injected_simultaneous=0;break;
                case 12:v.adc1_jdr1=4096;break;
                case 13:v.sample_window_valid=0;break;
            }
            if(tc!=5) REQUIRE(!foc_adc_contract_complete(&g,&v,&out));
        }
        REQUIRE(g.fault_latched && !g.pending && g.last_fault!=0u);
        REQUIRE(!out.valid && out.counts.ia==0 && out.counts.ib==0 && out.counts.ic==0);
        REQUIRE(!foc_adc_contract_prepare(&g,1,11,off,1));
        REQUIRE(!foc_adc_contract_reset(&g,0));
        REQUIRE(g.fault_latched);
        REQUIRE(foc_adc_contract_reset(&g,1));
        REQUIRE(!g.fault_latched && g.rejected==0u);
    }
    /* Duplicate and skipped sequences must latch, even if the ADC values look sane. */
    for(unsigned i=0;i<2;i++) {
        foc_adc_contract_t g={0};foc_adc_result_t out={0};
        REQUIRE(foc_adc_contract_prepare(&g,1,55,off,1));
        foc_adc_sample_t v=sample(1,55);
        REQUIRE(foc_adc_contract_complete(&g,&v,&out));
        REQUIRE(!foc_adc_contract_prepare(&g,2,i?57:55,off,1));
        REQUIRE(g.last_fault==FOC_ADC_BAD_SEQUENCE && g.fault_latched);
    }
    /* A second completion without a corresponding PWM event is invalid. */
    {
        foc_adc_contract_t g={0};foc_adc_result_t out={0};
        REQUIRE(foc_adc_contract_prepare(&g,1,1,off,1));
        foc_adc_sample_t v=sample(1,1);
        REQUIRE(foc_adc_contract_complete(&g,&v,&out));
        REQUIRE(!foc_adc_contract_complete(&g,&v,&out));
        REQUIRE(!out.valid && g.fault_latched);
    }
    return 0;
}
int main(void) {
    if(positive()||negative()) return 1;
    puts("PASS FOC ADC: 6 sector pairs, event ownership, wrap, 14 fail-closed cases, stale/duplicate frames");
    return 0;
}
