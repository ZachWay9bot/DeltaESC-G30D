#include "stock_dual_adc_plan.h"

static void set_pair(stock_dual_adc_plan_t *p, uint8_t sector) {
    stock_adc_pair_t pair=stock_current_pair_for_sector(sector);
    p->adc1_channel=pair.adc1_channel;
    p->adc2_channel=pair.adc2_channel;
    p->adc1_jsqr=stock_current_jsqr_one(pair.adc1_channel);
    p->adc2_jsqr=stock_current_jsqr_one(pair.adc2_channel);
}

stock_dual_adc_plan_t stock_dual_adc_plan_init(uint8_t sector) {
    stock_dual_adc_plan_t p={0};
    p.rcc_apb2_enable=STOCK_RCC_APB2_ADC1EN|STOCK_RCC_APB2_ADC2EN;
    p.adc1_cr1=STOCK_ADC_MODE_REG_INJEC_SIMULT|STOCK_ADC_CR1_JEOCIE;
    p.adc2_cr1=0u;
    p.adc1_cr2=STOCK_ADC_CR2_ADON|STOCK_ADC_CR2_JEXTSEL_TIM1_CC4|STOCK_ADC_CR2_JEXTTRIG;
    p.adc2_cr2=STOCK_ADC_CR2_ADON;
    set_pair(&p,sector);
    return p;
}

void stock_dual_adc_plan_next(stock_dual_adc_plan_t *p, uint8_t sector) {
    set_pair(p,sector);
}
