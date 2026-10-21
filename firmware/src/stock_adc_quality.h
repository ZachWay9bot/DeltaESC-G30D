#pragma once
#include <stdint.h>

/* Passive ADC-integrity checks: do not assume motor shunt gain or direction. */
#define ADCQ_RAW_INVALID 0x01u
#define ADCQ_PAIR_MISMATCH 0x02u
#define ADCQ_GAP_LONG 0x04u
#define ADCQ_INTERVAL_SHORT 0x08u
#define ADCQ_BAD_PAIR (ADCQ_RAW_INVALID | ADCQ_PAIR_MISMATCH)

typedef struct {
    uint16_t invalid_raw_events;
    uint16_t wrong_channel_events;
    uint16_t long_gap_events;
    uint16_t short_gap_events;
    uint32_t total_checked;
    uint8_t last_flags;
    uint8_t fault_latched;
} stock_adc_quality_t;

/* Zero interval is the first sample; raw values are 12-bit ADC samples.
 * Timing counters are diagnostic only, not hardware qualification. */
uint8_t stock_adc_quality_check(stock_adc_quality_t *q, uint8_t sector,
                                uint16_t raw1, uint16_t raw2,
                                uint32_t adc1_jsqr, uint32_t adc2_jsqr,
                                uint32_t interval_cycles,
                                uint32_t nominal_period_cycles);
