/* DeltaESC v0.9.18: passive G30D ADC evidence for BLE Motor Bench.
 * Voltage/current hardware qualification is not inferred from stock counts.
 * GPL-3.0-or-later.
 */
#ifndef G30_ADC_PROOF_H
#define G30_ADC_PROOF_H
#include <stdint.h>
#define G30_PROOF_SAMPLE_PRESENT  0x01u
#define G30_PROOF_ADC1_OFFSET_OK  0x02u
#define G30_PROOF_ADC2_OFFSET_OK  0x04u
#define G30_PROOF_RAW12_OK       0x08u
#define G30_PROOF_FRESH          0x10u
#define G30_PROOF_COAST_CCER     0x20u
#define G30_PROOF_KILLED         0x40u
#define G30_PROOF_ZERO_COUNTS    0x80u

typedef struct {
    uint16_t raw[3];
    uint16_t off[3];
    uint16_t adc2_ch4_off,adc2_ch5_off;
    uint16_t ccer;
    uint8_t have_sample,adc1_offsets_valid,adc2_ch4_valid,adc2_ch5_valid;
    uint8_t killed,powered;
    uint32_t now_ms,last_adc_ms;
} g30_adc_proof_input_t;
typedef struct {
    uint8_t flags;
    uint16_t peak_abs_counts;
    uint16_t age_ms;
} g30_adc_proof_result_t;

static inline uint8_t g30_adc_proof_off_ok(uint16_t v) {
    return (uint8_t)(v>=1948u && v<=2148u);
}
static inline uint16_t g30_adc_proof_abs(uint16_t a,uint16_t b) {
    return a>=b ? (uint16_t)(a-b) : (uint16_t)(b-a);
}
static inline g30_adc_proof_result_t
g30_adc_proof_eval(const g30_adc_proof_input_t *x) {
    g30_adc_proof_result_t o={0,0,0};
    uint8_t all_raw_ok=1u,all_off_ok=x->adc1_offsets_valid;
    uint16_t peak=0u;
    for(unsigned i=0;i<3u;i++) {
        if(x->raw[i]>4095u)all_raw_ok=0u;
        if(!g30_adc_proof_off_ok(x->off[i]))all_off_ok=0u;
        const uint16_t d=g30_adc_proof_abs(x->raw[i],x->off[i]);
        if(d>peak)peak=d;
    }
    o.peak_abs_counts=peak;
    const uint32_t age=x->now_ms-x->last_adc_ms;
    o.age_ms=age>65535u?65535u:(uint16_t)age;
    if(x->have_sample)o.flags|=G30_PROOF_SAMPLE_PRESENT;
    if(all_off_ok)o.flags|=G30_PROOF_ADC1_OFFSET_OK;
    if(x->adc2_ch4_valid && x->adc2_ch5_valid &&
       g30_adc_proof_off_ok(x->adc2_ch4_off) &&
       g30_adc_proof_off_ok(x->adc2_ch5_off))
        o.flags|=G30_PROOF_ADC2_OFFSET_OK;
    if(all_raw_ok)o.flags|=G30_PROOF_RAW12_OK;
    if(x->have_sample && age<=100u)o.flags|=G30_PROOF_FRESH;
    /* CH4 only does not prove electrically measured gate-off. */
    if(!x->powered && x->ccer==0x1000u)
        o.flags|=G30_PROOF_COAST_CCER;
    if(x->killed)o.flags|=G30_PROOF_KILLED;
    /* This indicates *raw unpowered* drift, never calibrated mA. */
    if((o.flags & (G30_PROOF_SAMPLE_PRESENT|G30_PROOF_ADC1_OFFSET_OK|
                   G30_PROOF_RAW12_OK|G30_PROOF_FRESH|
                   G30_PROOF_COAST_CCER)) ==
                  (G30_PROOF_SAMPLE_PRESENT|G30_PROOF_ADC1_OFFSET_OK|
                   G30_PROOF_RAW12_OK|G30_PROOF_FRESH|
                   G30_PROOF_COAST_CCER) && peak<=24u)
        o.flags|=G30_PROOF_ZERO_COUNTS;
    return o;
}
static inline void g30_adc_proof_u16(uint8_t *p,uint16_t v) {
    p[0]=(uint8_t)v;p[1]=(uint8_t)(v>>8);
}
/* Exactly 18 bytes per existing Ninebot BLE register page. */
static inline void g30_adc_proof_page_fa(uint8_t *p,
    const g30_adc_proof_input_t *x,g30_adc_proof_result_t o,uint16_t fault) {
    p[0]=1u;p[1]=o.flags;
    for(unsigned i=0;i<3u;i++) {
        g30_adc_proof_u16(p+2u+i*2u,x->raw[i]);
        g30_adc_proof_u16(p+8u+i*2u,x->off[i]);
    }
    g30_adc_proof_u16(p+14u,o.age_ms);
    g30_adc_proof_u16(p+16u,fault);
}
static inline void g30_adc_proof_page_fb(uint8_t *p,
    const g30_adc_proof_input_t *x,g30_adc_proof_result_t o,
    uint16_t bdtr,uint16_t vbus_raw,uint16_t i_last,uint16_t i_peak) {
    p[0]=1u;p[1]=o.flags;
    g30_adc_proof_u16(p+2u,x->adc2_ch4_off);
    g30_adc_proof_u16(p+4u,x->adc2_ch5_off);
    g30_adc_proof_u16(p+6u,i_last);
    g30_adc_proof_u16(p+8u,i_peak);
    g30_adc_proof_u16(p+10u,x->ccer);
    g30_adc_proof_u16(p+12u,bdtr);
    g30_adc_proof_u16(p+14u,vbus_raw);
    g30_adc_proof_u16(p+16u,o.peak_abs_counts);
}
#endif
