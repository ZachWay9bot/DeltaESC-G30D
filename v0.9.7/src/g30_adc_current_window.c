#include "g30_adc_current_window.h"
#include "stock_current_frontend.h"

#define G30_ARR 1999u
#define G30_SAMPLE_TICK 1800u
#define G30_ADC_SAMPLE_CODE 2u /* 13.5 ADC clocks, RM0008 */
#define G30_ADC_PRESCALER_BITS (2u << 14) /* PCLK2 / 6 => 10.667 MHz */
#define G30_ADC_CLOCKS_FOR_CONVERSION 26u /* 13.5 sample + 12.5 conversion */
#define G30_TIMER_PER_ADC_CLOCK 6u
#define G30_ADC_CONVERT_TIMER_TICKS (G30_ADC_CLOCKS_FOR_CONVERSION * G30_TIMER_PER_ADC_CLOCK)
#define G30_DEADTIME_TIMER_TICKS 64u
#define G30_EXTRA_SETTLE_TICKS 32u
#define G30_SAFE_LOW_SIDE_MARGIN (G30_ADC_CONVERT_TIMER_TICKS + G30_DEADTIME_TIMER_TICKS + G30_EXTRA_SETTLE_TICKS)
#define G30_TURNAROUND_MARGIN (G30_ADC_CONVERT_TIMER_TICKS + G30_EXTRA_SETTLE_TICKS)
#define ADC_DUALMODE_MASK (15u << 16)
#define ADC_DUALMODE_INJECTED_ONLY (5u << 16)
#define ADC_JEOCIE (1u << 7)
#define ADC_ADON (1u << 0)
#define ADC_JEXTSEL_MASK (7u << 12)
#define ADC_JEXTSEL_TIM1_CC4 (1u << 12)
#define ADC_JEXTSEL_SWSTART (7u << 12)
#define ADC_JEXTTRIG (1u << 15)
#define TIM_CENTER_MASK (3u << 5)
#define TIM_CEN (1u << 0)
#define TIM_CC4E (1u << 12)
#define TIM_CC4P (1u << 13)

static uint32_t smpr(uint32_t reg, uint8_t channel) {
    return (reg >> (3u * channel)) & 7u;
}

uint32_t g30_adc_current_flags(const g30_adc_current_state_t *s) {
    if (!s) return 0x7fu;
    uint32_t f=0;
    if ((s->rcc_cfgr & (3u<<14)) != G30_ADC_PRESCALER_BITS)
        f |= G30_ADC_F_CLOCK;
    if ((s->adc1_cr1 & ADC_DUALMODE_MASK) != ADC_DUALMODE_INJECTED_ONLY ||
        (s->adc2_cr1 & ADC_DUALMODE_MASK) != 0u ||
        !(s->adc1_cr1 & ADC_JEOCIE))
        f |= G30_ADC_F_DUAL;
    if (!(s->adc1_cr2 & (ADC_ADON|ADC_JEXTTRIG)) ||
        !(s->adc2_cr2 & (ADC_ADON|ADC_JEXTTRIG)) ||
        (s->adc1_cr2 & (ADC_ADON|ADC_JEXTTRIG)) != (ADC_ADON|ADC_JEXTTRIG) ||
        (s->adc2_cr2 & (ADC_ADON|ADC_JEXTTRIG)) != (ADC_ADON|ADC_JEXTTRIG) ||
        (s->adc1_cr2 & ADC_JEXTSEL_MASK) != ADC_JEXTSEL_TIM1_CC4 ||
        (s->adc2_cr2 & ADC_JEXTSEL_MASK) != ADC_JEXTSEL_SWSTART)
        f |= G30_ADC_F_TRIGGER;
    /* All three physical shunt channels CH3/4/5 have identical acquisition
     * times on both ADCs, no matter which sector selects which pair. */
    for (uint8_t ch=3u;ch<=5u;ch++) {
        if (smpr(s->adc1_smpr2,ch)!=G30_ADC_SAMPLE_CODE ||
            smpr(s->adc2_smpr2,ch)!=G30_ADC_SAMPLE_CODE)
            f |= G30_ADC_F_SAMPLE;
    }
    if (!(s->tim1_cr1 & TIM_CEN) || !(s->tim1_cr1 & TIM_CENTER_MASK) ||
        s->tim1_arr!=G30_ARR || s->tim1_ccr4!=G30_SAMPLE_TICK ||
        !(s->tim1_ccer & TIM_CC4E) || (s->tim1_ccer & TIM_CC4P))
        f |= G30_ADC_F_TIM1;
    if (s->sector<1u || s->sector>6u) {
        f |= G30_ADC_F_SECTOR;
    } else {
        const stock_adc_pair_t p=stock_current_pair_for_sector(s->sector);
        const uint16_t ccr[3]={s->tim1_ccr1,s->tim1_ccr2,s->tim1_ccr3};
        const uint16_t sampled_a=ccr[p.adc1_channel-3u];
        const uint16_t sampled_b=ccr[p.adc2_channel-3u];
        /* Works for either compare travel direction: selected LOW-side
         * shunts have time to settle and complete conversion before the
         * next PWM edge. For low-side sample topology only. */
        if (sampled_a > G30_SAMPLE_TICK-G30_SAFE_LOW_SIDE_MARGIN ||
            sampled_b > G30_SAMPLE_TICK-G30_SAFE_LOW_SIDE_MARGIN ||
            (G30_ARR-G30_SAMPLE_TICK)<G30_TURNAROUND_MARGIN)
            f |= G30_ADC_F_WINDOW;
    }
    return f;
}