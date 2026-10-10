#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "g30_adc_current_window.h"
#include "stock_current_frontend.h"

#define SMPR2 ((2u<<9)|(2u<<12)|(2u<<15))
static g30_adc_current_state_t fixture(uint8_t sector) {
    g30_adc_current_state_t s={0};
    s.rcc_cfgr=(2u<<14);
    s.adc1_cr1=(5u<<16)|(1u<<7)|(1u<<8);
    s.adc2_cr1=(1u<<8);
    s.adc1_cr2=1u|(1u<<12)|(1u<<15);
    s.adc2_cr2=1u|(7u<<12)|(1u<<15);
    s.adc1_smpr2=SMPR2;
    s.adc2_smpr2=SMPR2;
    s.tim1_cr1=(1u<<0)|(1u<<5)|(1u<<7);
    s.tim1_ccer=0x1555u;
    s.tim1_arr=1999u;
    s.tim1_ccr4=1800u;
    s.tim1_ccr1=1100u;
    s.tim1_ccr2=1100u;
    s.tim1_ccr3=1100u;
    s.sector=sector;
    return s;
}
static void check_current_model(void) {
    for (uint8_t sector=1u; sector<=6u;sector++) {
        const stock_adc_pair_t pair=stock_current_pair_for_sector(sector);
        assert(pair.adc1_channel!=pair.adc2_channel);
        assert(pair.adc1_channel>=3u&&pair.adc1_channel<=5u);
        assert(pair.adc2_channel>=3u&&pair.adc2_channel<=5u);
        const uint16_t offset[3]={2040u,2070u,2080u};
        const uint16_t v1=(uint16_t)(offset[pair.adc1_channel-3u]+100u);
        const uint16_t v2=(uint16_t)(offset[pair.adc2_channel-3u]-40u);
        const stock_phase_currents_t v=stock_current_reconstruct_counts(sector,v1,v2,offset);
        assert((int32_t)v.ia+v.ib+v.ic==0);
        assert(v.ia>=-140 && v.ia<=140);
        assert(v.ib>=-140 && v.ib<=140);
        assert(v.ic>=-140 && v.ic<=140);
        g30_adc_current_state_t s=fixture(sector);
        assert(g30_adc_current_flags(&s)==0u);
        s.tim1_ccr1=1700u;s.tim1_ccr2=1700u;s.tim1_ccr3=1700u;
        assert(g30_adc_current_flags(&s)&G30_ADC_F_WINDOW);
    }
    assert(stock_current_nominal_counts_to_ma(100)==5036);
    assert(stock_current_nominal_counts_to_ma(-100)==-5036);
    assert(stock_current_nominal_ma_to_counts(500)==9);
    assert(stock_current_scaled_unit_from_delta(100)==stock_current_nominal_counts_to_ma(100));
}
int main(void) {
    check_current_model();
    g30_adc_current_state_t s=fixture(1u);
    assert(g30_adc_current_flags(&s)==0u);
    s.rcc_cfgr=0u;assert(g30_adc_current_flags(&s)&G30_ADC_F_CLOCK);s=fixture(1u);
    s.adc1_cr1=0u;assert(g30_adc_current_flags(&s)&G30_ADC_F_DUAL);s=fixture(1u);
    s.adc2_cr1=(1u<<16);assert(g30_adc_current_flags(&s)&G30_ADC_F_DUAL);s=fixture(1u);
    s.adc1_cr2=1u;assert(g30_adc_current_flags(&s)&G30_ADC_F_TRIGGER);s=fixture(1u);
    s.adc2_cr2=1u;assert(g30_adc_current_flags(&s)&G30_ADC_F_TRIGGER);s=fixture(1u);
    s.adc2_smpr2^=(1u<<12);assert(g30_adc_current_flags(&s)&G30_ADC_F_SAMPLE);s=fixture(1u);
    s.tim1_ccer|=(1u<<13);assert(g30_adc_current_flags(&s)&G30_ADC_F_TIM1);s=fixture(1u);
    s.tim1_ccer&=~(1u<<12);assert(g30_adc_current_flags(&s)&G30_ADC_F_TIM1);s=fixture(1u);
    s.tim1_cr1&=~(3u<<5);assert(g30_adc_current_flags(&s)&G30_ADC_F_TIM1);s=fixture(1u);
    s.tim1_ccr4=1750u;assert(g30_adc_current_flags(&s)&G30_ADC_F_TIM1);s=fixture(1u);
    s.sector=0u;assert(g30_adc_current_flags(&s)&G30_ADC_F_SECTOR);s=fixture(1u);
    s.tim1_ccr2=1549u;assert(g30_adc_current_flags(&s)&G30_ADC_F_WINDOW);s=fixture(1u);
    s.tim1_ccr3=1549u;assert(g30_adc_current_flags(&s)&G30_ADC_F_WINDOW);s=fixture(1u);
    s.tim1_ccr1=1549u;assert(g30_adc_current_flags(&s)==0u); /* A not sampled in sector 1 */
    assert(g30_adc_current_flags(NULL)!=0u);
    puts("PASS G30D ADC timing + window + 6 sectors + nominal-current sign symmetry");
    return 0;
}