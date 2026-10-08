#pragma once
#include <stdint.h>

typedef struct {
    int16_t ia;
    int16_t ib;
    int16_t ic;
} stock_phase_currents_t;

typedef struct {
    uint8_t adc1_channel;
    uint8_t adc2_channel;
} stock_adc_pair_t;

uint32_t stock_current_jsqr_one(uint8_t channel);
stock_adc_pair_t stock_current_pair_for_sector(uint8_t sector);
uint8_t stock_current_sector_from_ab(int32_t a, int32_t b);
stock_phase_currents_t stock_current_reconstruct_counts(uint8_t sector,
                                                        uint16_t adc1_raw,
                                                        uint16_t adc2_raw,
                                                        const uint16_t phase_offset[3]);
int32_t stock_current_scaled_unit_from_delta(int32_t adc_delta);
