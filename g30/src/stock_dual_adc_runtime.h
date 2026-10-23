#pragma once
#include <stdint.h>
#include "stock_current_frontend.h"
#define STOCK_DUAL_OFFSET_SAMPLES 512u
typedef struct {
    uint16_t raw[3];
    uint16_t offsets[3];
    uint32_t cal_sum[3];
    uint16_t cal_samples[3];
    uint8_t sector;
    uint8_t calibrating;
    uint8_t offset_valid;
} stock_dual_adc_runtime_t;
void stock_dual_adc_runtime_init(stock_dual_adc_runtime_t *p,const uint16_t offset[3]);
void stock_dual_adc_runtime_begin_offset(stock_dual_adc_runtime_t *p);
void stock_dual_adc_runtime_abort_offset(stock_dual_adc_runtime_t *p);
uint8_t stock_dual_adc_runtime_capture(stock_dual_adc_runtime_t *p,uint16_t raw_adc1,uint16_t raw_adc2);
void stock_dual_adc_runtime_next_sector(stock_dual_adc_runtime_t *p,uint8_t sector);
stock_phase_currents_t stock_dual_adc_runtime_currents(const stock_dual_adc_runtime_t *p,uint16_t raw_adc1,uint16_t raw_adc2);
uint8_t stock_dual_adc_runtime_rotate(uint8_t sector);
