#include "stock_adc_quality.h"
#include "stock_current_frontend.h"

static void inc_sat(uint16_t *p) { if(*p<65535u)++*p; }

uint8_t stock_adc_quality_check(stock_adc_quality_t *q, uint8_t sector,
                                uint16_t raw1, uint16_t raw2,
                                uint32_t adc1_jsqr, uint32_t adc2_jsqr,
                                uint32_t interval_cycles,
                                uint32_t nominal_period_cycles){
    if (!q) return ADCQ_BAD_PAIR;
    uint8_t f=0u;
    if (raw1>4095u || raw2>4095u) {
        f|=ADCQ_RAW_INVALID;
        inc_sat(&q->invalid_raw_events);
    }
    if (sector<1u || sector>6u) {
        f|=ADCQ_PAIR_MISMATCH;
    } else {
        const stock_adc_pair_t p=stock_current_pair_for_sector(sector);
        if (adc1_jsqr!=stock_current_jsqr_one(p.adc1_channel) ||
            adc2_jsqr!=stock_current_jsqr_one(p.adc2_channel))
            f|=ADCQ_PAIR_MISMATCH;
    }
    if (f&ADCQ_PAIR_MISMATCH) inc_sat(&q->wrong_channel_events);
    /* Diagnostic jitter counting. Actual gate timing is still unvalidated. */
    if (nominal_period_cycles && interval_cycles) {
        if (interval_cycles>nominal_period_cycles*2u) {
            f|=ADCQ_GAP_LONG; inc_sat(&q->long_gap_events);
        } else if (interval_cycles<nominal_period_cycles/2u) {
            f|=ADCQ_INTERVAL_SHORT; inc_sat(&q->short_gap_events);
        }
    }
    q->last_flags=f;
    if (f&ADCQ_BAD_PAIR) q->fault_latched=1u;
    q->total_checked++;
    return f;
}
