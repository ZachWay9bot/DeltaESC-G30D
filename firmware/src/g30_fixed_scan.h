#pragma once
#include <stdint.h>
/* Fixed two-rank dual injected scan. ADC1 CH3 -> CH4, ADC2 CH5 -> CH5.
 * No sector-dependent JSQR changes while ADCs operate. GATES OFF ONLY.
 * This is raw diagnostic capture, not synchronous FOC current qualification. */
#define G30_FIXED_JSQR_A ((1u<<20)|(3u<<10)|(4u<<15))
#define G30_FIXED_JSQR_B ((1u<<20)|(5u<<10)|(5u<<15))
#define G30_FIXED_OFFSET_SAMPLES 512u
/* Conservative DIAGNOSTIC limits in RAW ADC counts, NOT motor-ready calibration. */
#define G30_FIXED_CAL_MAX_P2P_COUNTS 64u
#define G30_FIXED_CAL_MAX_DUP_DELTA_COUNTS 32u
#define G30_SCAN_BAD_MODE   0x01u
#define G30_SCAN_BAD_JSQR   0x02u
#define G30_SCAN_BAD_RAW    0x04u
#define G30_SCAN_DUP_SPREAD 0x08u
#define G30_SCAN_NO_JEOC    0x10u
#define G30_SCAN_CAL_UNSTABLE 0x20u
#define G30_SCAN_FATAL_MASK (G30_SCAN_BAD_MODE|G30_SCAN_BAD_JSQR|G30_SCAN_BAD_RAW|G30_SCAN_NO_JEOC|G30_SCAN_CAL_UNSTABLE)
typedef struct {
    uint16_t raw[3],offsets[3],c_repeat;
    uint16_t cal_min[3],cal_max[3],cal_max_dup_delta;
    uint32_t sums[3],samples;
    uint16_t cal_count,bad_count;
    uint8_t calibrating,offset_valid,last_flags;
} g30_fixed_scan_t;
void g30_fixed_scan_init(g30_fixed_scan_t *s,const uint16_t initial[3]);
void g30_fixed_scan_start_offsets(g30_fixed_scan_t *s);
void g30_fixed_scan_abort(g30_fixed_scan_t *s);
/* Must only call with register values sampled inside completed ADC1_2 IRQ. */
uint8_t g30_fixed_scan_capture(g30_fixed_scan_t *s,uint32_t jsqr_a,uint32_t jsqr_b,
        uint32_t adc1_cr1,uint32_t adc2_cr1,uint16_t adc1_sr,uint16_t adc2_sr,
        uint16_t a,uint16_t b,uint16_t c,uint16_t c_again);
void g30_fixed_scan_encode_ef(const g30_fixed_scan_t *s,uint8_t out[16]);
/* F6 read-only: calibrated raw spans and worst CH5 dual-rank disagreement. */
void g30_fixed_scan_encode_f6(const g30_fixed_scan_t *s,uint8_t out[16]);
