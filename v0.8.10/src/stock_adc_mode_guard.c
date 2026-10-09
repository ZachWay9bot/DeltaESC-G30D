#include "stock_adc_mode_guard.h"
static void le16(uint8_t *p,uint16_t x){p[0]=(uint8_t)x;p[1]=(uint8_t)(x>>8);}
static void le32(uint8_t *p,uint32_t x){le16(p,(uint16_t)x);le16(p+2,(uint16_t)(x>>16));}
uint8_t g30_adc_mode_flags(uint32_t master,uint32_t slave,uint32_t slave_sr){
    uint8_t f=0;
    if((master&G30_ADC_DUALMOD_MASK)!=G30_ADC_DUALMOD_INJECTED_ONLY)
        f|=G30_ADCMODE_BAD_MASTER;
    if(slave&G30_ADC_DUALMOD_MASK)f|=G30_ADCMODE_BAD_SLAVE;
    if(!(slave_sr&G30_ADC_JEOC))f|=G30_ADCMODE_ADC2_NOT_READY;
    return f;
}
uint8_t g30_adc_mode_capture(g30_adc_mode_guard_t *g,uint32_t adc1_cr1,
                             uint32_t adc2_cr1,uint32_t adc1_cr2,uint32_t adc2_sr){
    if(!g)return G30_ADCMODE_BAD_MASTER|G30_ADCMODE_BAD_SLAVE|
                 G30_ADCMODE_ADC2_NOT_READY;
    const uint8_t f=g30_adc_mode_flags(adc1_cr1,adc2_cr1,adc2_sr);
    g->adc1_cr1=adc1_cr1;g->adc2_cr1=adc2_cr1;g->adc1_cr2=adc1_cr2;
    g->adc2_sr=(uint16_t)adc2_sr;
    g->last_flags=f;
    if(f){g->ever_faulted=1; if(g->fault_count!=0xffffu)g->fault_count++;}
    return f;
}
void g30_adc_mode_encode_ee(const g30_adc_mode_guard_t *g,uint8_t out[16]){
    if(!out)return;
    if(!g){for(unsigned i=0;i<16;i++)out[i]=0;return;}
    le32(out,g->adc1_cr1);le32(out+4,g->adc2_cr1);
    le32(out+8,g->adc1_cr2);
    le16(out+12,g->fault_count);
    out[14]=g->last_flags;out[15]=g->ever_faulted;
}
