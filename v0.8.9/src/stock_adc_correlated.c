#include "stock_adc_correlated.h"
#include "stock_adc_quality.h"
static void le16(uint8_t *p,uint16_t v){p[0]=(uint8_t)v;p[1]=(uint8_t)(v>>8);}
static void le32(uint8_t *p,uint32_t v){le16(p,(uint16_t)v);le16(p+2,(uint16_t)(v>>16));}

void stock_adc_correlated_capture(stock_adc_correlated_t *out,
                                 const stock_adc_snapshot_t *adc,
                                 const stock_adc_timing_t *tim,
                                 uint8_t quality_flags){
    if(!out||!adc||!tim)return;
    /* Called only from ADC IRQ after both individual snapshots, before
       writing JSQR for next sector. No later live hardware register reads. */
    out->sequence=tim->sample_sequence;
    out->adc1_raw=adc->adc1_raw;
    out->adc2_raw=adc->adc2_raw;
    out->tim1_cnt=tim->cnt;
    out->tim1_ccr4=tim->ccr4;
    out->sector=adc->sector;
    out->adc1_channel=(uint8_t)((adc->adc1_jsqr>>15)&0x1fu);
    out->adc2_channel=(uint8_t)((adc->adc2_jsqr>>15)&0x1fu);
    uint8_t flags=0u;
    if(adc->adc1_sr&(1u<<2))flags|=ADC_ED_ADC1_JEOC;
    if(adc->adc2_sr&(1u<<2))flags|=ADC_ED_ADC2_JEOC;
    if(quality_flags&ADCQ_BAD_PAIR)flags|=ADC_ED_BAD_PAIR;
    if(tim->config_flags&ADC_TIMING_GATE_ENABLED)flags|=ADC_ED_GATE_ENABLED;
    if(quality_flags&(ADCQ_GAP_LONG|ADCQ_INTERVAL_SHORT))
        flags|=ADC_ED_INTERVAL_ANOMALY;
    if(tim->config_flags&~ADC_TIMING_GATE_ENABLED)
        flags|=ADC_ED_TIM1_CONFIG_ANOMALY;
    out->flags=flags;
}
void stock_adc_correlated_encode_ed(const stock_adc_correlated_t *in,
                                   uint8_t out[16]){
    if(!out)return;
    if(!in){for(unsigned i=0;i<16u;i++)out[i]=0u;return;}
    le32(out,in->sequence);
    le16(out+4,in->adc1_raw);le16(out+6,in->adc2_raw);
    le16(out+8,in->tim1_cnt);le16(out+10,in->tim1_ccr4);
    out[12]=in->sector;
    out[13]=in->adc1_channel;out[14]=in->adc2_channel;
    out[15]=in->flags;
}
