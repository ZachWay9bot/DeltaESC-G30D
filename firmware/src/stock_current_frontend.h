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

/* Internal scaled units reproduced byte-for-byte from DRV126's
 * signed MUL + ASRS #10. They are not independently calibrated amperes. */
typedef struct { int32_t ia, ib, ic; } stock_drv126_scaled_currents_t;
int32_t stock_current_drv126_scaled_delta(int32_t adc_delta);
stock_drv126_scaled_currents_t stock_current_drv126_reconstruct_scaled(
    uint8_t sector, int32_t adc1_delta, int32_t adc2_delta);
uint32_t stock_current_jsqr_one(uint8_t channel);
stock_adc_pair_t stock_current_pair_for_sector(uint8_t sector);
uint8_t stock_current_sector_from_ab(int32_t a, int32_t b);
stock_phase_currents_t stock_current_reconstruct_counts(uint8_t sector,
                                                        uint16_t adc1_raw,
                                                        uint16_t adc2_raw,
                                                        const uint16_t phase_offset[3]);
/* Shared nominal conversion used by CURRENT CONTROL and observer.
 * Provisional until DRV126 analogue shunt/gain interpretation is proven.
 * 51575/1024 mA/count is NOT a hardware current-calibration guarantee. */
int32_t stock_current_nominal_counts_to_ma(int32_t counts);
int16_t stock_current_nominal_ma_to_counts(uint16_t ma);
int32_t stock_current_scaled_unit_from_delta(int32_t adc_delta);
