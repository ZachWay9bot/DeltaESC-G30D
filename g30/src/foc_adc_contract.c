#include "foc_adc_contract.h"
#include <stddef.h>

#define ADC_JEOC_BIT (1u << 2)

static void result_clear(foc_adc_result_t *out) {
    if (!out) return;
    out->counts.ia = 0; out->counts.ib = 0; out->counts.ic = 0;
    out->sequence = 0u; out->sector = 0u; out->valid = 0u;
}

static uint8_t reject(foc_adc_contract_t *c, uint8_t reason) {
    if (c) {
        c->pending = 0u;
        c->last_fault = reason;
        c->fault_latched = 1u;
        if (c->rejected != 0xffffffffu) c->rejected++;
    }
    return 0u;
}

void foc_adc_contract_init(foc_adc_contract_t *c) {
    if (!c) return;
    *c = (foc_adc_contract_t){0};
}

uint8_t foc_adc_contract_reset(foc_adc_contract_t *c, uint8_t bridge_channels_disabled) {
    if (!c || !bridge_channels_disabled) return 0u;
    foc_adc_contract_init(c);
    return 1u;
}

uint8_t foc_adc_contract_prepare(foc_adc_contract_t *c, uint8_t sector,
                                 uint32_t sequence, const uint16_t offsets[3],
                                 uint8_t hw_qualified) {
    if (!c || c->fault_latched) return 0u;
    if (c->pending) return reject(c, FOC_ADC_BAD_STATE);
    if (!hw_qualified || !offsets) return reject(c, FOC_ADC_UNQUALIFIED);
    if (sector < 1u || sector > 6u) return reject(c, FOC_ADC_BAD_SECTOR);
    for (unsigned i = 0; i < 3u; i++)
        if (offsets[i] > 4095u) return reject(c, FOC_ADC_BAD_RAW);
    /* Every PWM sample event must complete once. Wrap is intentional. */
    if (c->seen_sample && sequence != c->last_seq + 1u)
        return reject(c, FOC_ADC_BAD_SEQUENCE);
    stock_adc_pair_t p = stock_current_pair_for_sector(sector);
    if (p.adc1_channel == p.adc2_channel)
        return reject(c, FOC_ADC_BAD_CHANNELS);
    c->expected_seq = sequence;
    c->expected_sector = sector;
    c->expected_jsqr1 = stock_current_jsqr_one(p.adc1_channel);
    c->expected_jsqr2 = stock_current_jsqr_one(p.adc2_channel);
    for (unsigned i = 0; i < 3u; i++) c->offsets[i] = offsets[i];
    c->pending = 1u;
    return 1u;
}

uint8_t foc_adc_contract_complete(foc_adc_contract_t *c,
                                  const foc_adc_sample_t *s,
                                  foc_adc_result_t *out) {
    result_clear(out);
    if (!c) return 0u;
    if (!out) return reject(c, FOC_ADC_BAD_STATE);
    if (c->fault_latched) return 0u;
    if (!c->pending || !s) return reject(c, FOC_ADC_BAD_STATE);
    /* IMPORTANT: verify the completed sample belongs to the PREVIOUSLY
       scheduled PWM sector. Comparing to the NEXT sector is a one-cycle bug. */
    if (s->sequence != c->expected_seq || s->sector != c->expected_sector)
        return reject(c, FOC_ADC_BAD_SEQUENCE);
    if (s->adc1_jsqr != c->expected_jsqr1 || s->adc2_jsqr != c->expected_jsqr2)
        return reject(c, FOC_ADC_BAD_CHANNELS);
    if (!(s->adc1_sr & ADC_JEOC_BIT) || !(s->adc2_sr & ADC_JEOC_BIT) ||
        !s->dual_injected_simultaneous)
        return reject(c, FOC_ADC_NOT_READY);
    if (s->adc1_jdr1 > 4095u || s->adc2_jdr1 > 4095u)
        return reject(c, FOC_ADC_BAD_RAW);
    if (!s->sample_window_valid) return reject(c, FOC_ADC_BAD_WINDOW);
    out->counts = stock_current_reconstruct_counts(c->expected_sector,
                    s->adc1_jdr1, s->adc2_jdr1, c->offsets);
    out->sequence = c->expected_seq;
    out->sector = c->expected_sector;
    out->valid = 1u;
    c->last_seq = c->expected_seq;
    c->seen_sample = 1u;
    c->pending = 0u;
    c->last_fault = 0u;
    if (c->accepted != 0xffffffffu) c->accepted++;
    return 1u;
}
