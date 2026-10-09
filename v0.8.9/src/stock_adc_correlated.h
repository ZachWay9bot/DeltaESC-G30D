#pragma once
#include <stdint.h>
#include "stock_adc_snapshot.h"
#include "stock_adc_timing.h"

/* ED: 16-byte, READ-ONLY evidence of ONE completed injected ADC interrupt.
 * This record never authorizes current calibration or power-stage operation. */
enum {
    ADC_ED_ADC1_JEOC=1u,
    ADC_ED_ADC2_JEOC=2u,
    ADC_ED_BAD_PAIR=4u,
    ADC_ED_GATE_ENABLED=8u,
    ADC_ED_INTERVAL_ANOMALY=16u,
    ADC_ED_TIM1_CONFIG_ANOMALY=32u
};
typedef struct {
    uint32_t sequence;
    uint16_t adc1_raw,adc2_raw,tim1_cnt,tim1_ccr4;
    uint8_t sector,adc1_channel,adc2_channel,flags;
} stock_adc_correlated_t;
void stock_adc_correlated_capture(stock_adc_correlated_t *out,
                                 const stock_adc_snapshot_t *adc,
                                 const stock_adc_timing_t *tim,
                                 uint8_t quality_flags);
void stock_adc_correlated_encode_ed(const stock_adc_correlated_t *in,
                                   uint8_t out[16]);
