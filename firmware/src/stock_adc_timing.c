#include "stock_adc_timing.h"
static void le16(uint8_t *p,uint16_t v){p[0]=(uint8_t)v;p[1]=(uint8_t)(v>>8);}
static void le32(uint8_t *p,uint32_t v){le16(p,(uint16_t)v);le16(p+2,(uint16_t)(v>>16));}
uint16_t stock_adc_timing_flags(uint16_t arr,uint16_t ccr4,
                                uint16_t cr1,uint16_t ccer,uint16_t bdtr){
    uint16_t f=0;
    if(!(cr1&1u))f|=ADC_TIMING_NO_CLOCK;
    if(!(cr1&(3u<<5)))f|=ADC_TIMING_NOT_CENTER;
    if(!(ccer&(1u<<12)))f|=ADC_TIMING_NO_CC4;
    if(ccer&0x555u)f|=ADC_TIMING_GATE_ENABLED;
    if(!arr||ccr4>arr)f|=ADC_TIMING_BAD_COMPARE;
    if(!(bdtr&(1u<<15)))f|=ADC_TIMING_NO_MOE;
    return f;
}
void stock_adc_timing_capture(stock_adc_timing_t *s,
                              uint16_t cnt,uint16_t ccr4,uint16_t arr,
                              uint16_t cr1,uint16_t ccer,uint16_t bdtr,
                              uint16_t tim_sr,uint32_t dwt_cycle){
    if(!s)return;
    s->cnt=cnt;s->ccr4=ccr4;s->arr=arr;s->cr1=cr1;
    s->ccer=ccer;s->bdtr=bdtr;s->tim_sr=tim_sr;
    const uint32_t previous=s->irq_dwt_cycle;
    s->irq_period_cycles=s->sample_sequence?dwt_cycle-previous:0u;
    s->irq_dwt_cycle=dwt_cycle;
    s->sample_sequence++;
    s->config_flags=stock_adc_timing_flags(arr,ccr4,cr1,ccer,bdtr);
    if(s->config_flags&&s->fault_count!=0xffffu)s->fault_count++;
}
void stock_adc_timing_encode_eb(const stock_adc_timing_t *s,uint8_t out[16]){
    if(!out)return;
    if(!s){for(unsigned i=0;i<16u;i++)out[i]=0;return;}
    le16(out,s->cnt);le16(out+2,s->ccr4);le16(out+4,s->arr);
    le16(out+6,s->cr1);le16(out+8,s->ccer);le16(out+10,s->bdtr);
    le32(out+12,s->irq_dwt_cycle);
}
void stock_adc_timing_encode_ec(const stock_adc_timing_t *s,uint8_t out[16]){
    if(!out)return;
    if(!s){for(unsigned i=0;i<16u;i++)out[i]=0;return;}
    le32(out,s->sample_sequence);le32(out+4,s->irq_period_cycles);
    le16(out+8,s->config_flags);le16(out+10,s->fault_count);
    le16(out+12,s->tim_sr);le16(out+14,0u);
}
