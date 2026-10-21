#pragma once
#include <stdint.h>
/* Evidence from the same ADC completion IRQ, BEFORE scheduling the next sector.
 * This is diagnostic evidence, not a qualified physical current measurement. */
typedef struct {
    uint32_t adc1_jsqr;
    uint32_t adc2_jsqr;
    uint16_t adc1_sr;
    uint16_t adc2_sr;
    uint16_t adc1_raw;
    uint16_t adc2_raw;
    uint8_t sector;
    uint32_t timestamp_ms;
} stock_adc_snapshot_t;

void stock_adc_snapshot_capture(stock_adc_snapshot_t *s, uint8_t sector,
                                uint16_t raw1, uint16_t raw2,
                                uint32_t jsqr1, uint32_t jsqr2,
                                uint16_t sr1, uint16_t sr2,
                                uint32_t timestamp_ms);
/* 16B EA diagnostic, little endian, full 32-bit JSQR for both ADCs */
void stock_adc_snapshot_encode_ea(const stock_adc_snapshot_t *s,uint8_t out[16]);
