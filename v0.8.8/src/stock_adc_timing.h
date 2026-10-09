#pragma once
#include <stdint.h>
/* IRQ-entry timer evidence. This is NOT the analog ADC sampling instant. */
typedef struct {
    uint16_t cnt,ccr4,arr,cr1,ccer,bdtr,tim_sr;
    uint16_t config_flags,fault_count;
    uint32_t irq_dwt_cycle,irq_period_cycles,sample_sequence;
} stock_adc_timing_t;
enum {
    ADC_TIMING_NO_CLOCK=1u,
    ADC_TIMING_NOT_CENTER=2u,
    ADC_TIMING_NO_CC4=4u,
    ADC_TIMING_GATE_ENABLED=8u,
    ADC_TIMING_BAD_COMPARE=16u,
    ADC_TIMING_NO_MOE=32u
};
uint16_t stock_adc_timing_flags(uint16_t arr,uint16_t ccr4,
                                uint16_t cr1,uint16_t ccer,uint16_t bdtr);
void stock_adc_timing_capture(stock_adc_timing_t *s,
                              uint16_t cnt,uint16_t ccr4,uint16_t arr,
                              uint16_t cr1,uint16_t ccer,uint16_t bdtr,
                              uint16_t tim_sr,uint32_t dwt_cycle);
void stock_adc_timing_encode_eb(const stock_adc_timing_t *s,uint8_t out[16]);
void stock_adc_timing_encode_ec(const stock_adc_timing_t *s,uint8_t out[16]);
