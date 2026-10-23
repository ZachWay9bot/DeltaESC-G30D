#include "g30_fixed_scan.h"
#include "stock_adc_mode_guard.h"
static void le16(uint8_t *p,uint16_t v){p[0]=(uint8_t)v;p[1]=(uint8_t)(v>>8);}
static uint16_t absdiff(uint16_t a,uint16_t b){return a>b?a-b:b-a;}
void g30_fixed_scan_init(g30_fixed_scan_t *s,const uint16_t initial[3]){
    if(!s||!initial)return;
    for(unsigned i=0;i<3u;i++){s->raw[i]=initial[i];s->offsets[i]=initial[i];s->sums[i]=0;}
    s->c_repeat=0u;s->samples=0u;s->cal_count=0u;s->bad_count=0u;
    s->calibrating=0u;s->offset_valid=0u;s->last_flags=0u;
    for(unsigned i=0u;i<3u;i++){s->cal_min[i]=4095u;s->cal_max[i]=0u;}
    s->cal_max_dup_delta=0u;
}
void g30_fixed_scan_start_offsets(g30_fixed_scan_t *s){
    if(!s)return;
    s->calibrating=1u;s->offset_valid=0u;s->cal_count=0u;
    for(unsigned i=0;i<3u;i++){
        s->sums[i]=0u;s->cal_min[i]=4095u;s->cal_max[i]=0u;
    }
    s->cal_max_dup_delta=0u;
}
void g30_fixed_scan_abort(g30_fixed_scan_t *s){
    if(!s)return;s->calibrating=0u;s->offset_valid=0u;
}
uint8_t g30_fixed_scan_capture(g30_fixed_scan_t *s,uint32_t jsqr_a,uint32_t jsqr_b,
        uint32_t adc1_cr1,uint32_t adc2_cr1,uint16_t adc1_sr,uint16_t adc2_sr,
        uint16_t a,uint16_t b,uint16_t c,uint16_t c_again){
    if(!s)return G30_SCAN_FATAL_MASK;
    uint8_t flags=0u;
    if((adc1_cr1&G30_ADC_DUALMOD_MASK)!=G30_ADC_DUALMOD_INJECTED_ONLY ||
       (adc2_cr1&G30_ADC_DUALMOD_MASK)!=0u ||
       !(adc1_cr1&(1u<<8)) || !(adc2_cr1&(1u<<8)))flags|=G30_SCAN_BAD_MODE;
    if(jsqr_a!=G30_FIXED_JSQR_A||jsqr_b!=G30_FIXED_JSQR_B)flags|=G30_SCAN_BAD_JSQR;
    if(a>4095u||b>4095u||c>4095u||c_again>4095u)flags|=G30_SCAN_BAD_RAW;
    if(!(adc1_sr&G30_ADC_JEOC)||!(adc2_sr&G30_ADC_JEOC))flags|=G30_SCAN_NO_JEOC;
    /* CH5 is measured at DIFFERENT injected ranks. A large deviation can
     * be electrical activity rather than noise; either way it must never
     * be interpreted as a trustworthy ZERO-CURRENT offset measurement. */
    const uint16_t duplicate_delta=absdiff(c,c_again);
    if((flags&G30_SCAN_FATAL_MASK)==0u && duplicate_delta>128u)
        flags|=G30_SCAN_DUP_SPREAD; /* Outside E0: warning only, gates OFF. */
    if((flags&G30_SCAN_FATAL_MASK)==0u && s->calibrating){
        if(duplicate_delta>s->cal_max_dup_delta)s->cal_max_dup_delta=duplicate_delta;
        if(duplicate_delta>G30_FIXED_CAL_MAX_DUP_DELTA_COUNTS)
            flags|=G30_SCAN_CAL_UNSTABLE;
        const uint16_t observed[3]={a,b,c};
        for(unsigned i=0u;i<3u;i++){
            uint16_t low=s->cal_min[i],high=s->cal_max[i];
            if(observed[i]<low)low=observed[i];
            if(observed[i]>high)high=observed[i];
            s->cal_min[i]=low;s->cal_max[i]=high;
            if((uint16_t)(high-low)>G30_FIXED_CAL_MAX_P2P_COUNTS)
                flags|=G30_SCAN_CAL_UNSTABLE;
        }
    }
    if((flags&G30_SCAN_FATAL_MASK)==0u){
        s->raw[0]=a;s->raw[1]=b;s->raw[2]=c;s->c_repeat=c_again;
        s->samples++;
        if(s->calibrating&&s->cal_count<G30_FIXED_OFFSET_SAMPLES){
            s->sums[0]+=a;s->sums[1]+=b;s->sums[2]+=c;
            if(++s->cal_count==G30_FIXED_OFFSET_SAMPLES){
                for(unsigned i=0;i<3u;i++)s->offsets[i]=(uint16_t)(s->sums[i]/G30_FIXED_OFFSET_SAMPLES);
                s->calibrating=0u;s->offset_valid=1u;
            }
        }
    } else {
        if(s->bad_count!=65535u)++s->bad_count;
        s->calibrating=0u;s->offset_valid=0u;
    }
    s->last_flags=flags;
    return flags;
}
void g30_fixed_scan_encode_ef(const g30_fixed_scan_t *s,uint8_t out[16]){
    if(!out)return;
    if(!s){for(unsigned i=0u;i<16u;i++)out[i]=0u;return;}
    for(unsigned i=0u;i<3u;i++){
        le16(out+2u*i,s->raw[i]);le16(out+6u+2u*i,s->offsets[i]);
    }
    le16(out+12u,s->c_repeat);
    out[14]=s->last_flags;
    out[15]=(uint8_t)((s->calibrating?1u:0u)|(s->offset_valid?2u:0u));
}

void g30_fixed_scan_encode_f6(const g30_fixed_scan_t *s,uint8_t out[16]){
    if(!out)return;
    if(!s){for(unsigned i=0u;i<16u;i++)out[i]=0u;return;}
    for(unsigned i=0u;i<3u;i++){
        le16(out+2u*i,s->cal_min[i]);
        le16(out+6u+2u*i,s->cal_max[i]);
    }
    le16(out+12u,s->cal_max_dup_delta);
    le16(out+14u,s->cal_count);
}
