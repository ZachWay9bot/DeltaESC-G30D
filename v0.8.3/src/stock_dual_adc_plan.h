#pragma once
#include <stdint.h>
#include "stock_current_frontend.h"

enum {
    STOCK_ADC1_BASE = 0x40012400u,
    STOCK_ADC2_BASE = 0x40012800u,
    STOCK_ADC_MODE_REG_INJEC_SIMULT = 0x00010000u,
    STOCK_ADC_CR1_JEOCIE = 1u << 7,
    STOCK_ADC_CR2_ADON = 1u << 0,
    STOCK_ADC_CR2_JEXTSEL_TIM1_CC4 = 1u << 12,
    STOCK_ADC_CR2_JEXTTRIG = 1u << 15,
    STOCK_RCC_APB2_ADC1EN = 1u << 9,
    STOCK_RCC_APB2_ADC2EN = 1u << 10
};

typedef struct {
    uint32_t rcc_apb2_enable;
    uint32_t adc1_cr1;
    uint32_t adc2_cr1;
    uint32_t adc1_cr2;
    uint32_t adc2_cr2;
    uint32_t adc1_jsqr;
    uint32_t adc2_jsqr;
    uint8_t adc1_channel;
    uint8_t adc2_channel;
} stock_dual_adc_plan_t;

stock_dual_adc_plan_t stock_dual_adc_plan_init(uint8_t sector);
void stock_dual_adc_plan_next(stock_dual_adc_plan_t *p, uint8_t sector);
