#include "stock_dual_adc_runtime.h"

void stock_dual_adc_runtime_init(stock_dual_adc_runtime_t *p,const uint16_t offset[3]) {
    p->sector=1u;p->calibrating=0u;p->offset_valid=0u;
    for(unsigned i=0u;i<3u;i++){
        p->raw[i]=offset[i];p->offsets[i]=offset[i];p->cal_sum[i]=0u;p->cal_samples[i]=0u;
    }
}
void stock_dual_adc_runtime_begin_offset(stock_dual_adc_runtime_t *p) {
    p->calibrating=1u;p->offset_valid=0u;
    for(unsigned i=0u;i<3u;i++){p->cal_sum[i]=0u;p->cal_samples[i]=0u;}
}
void stock_dual_adc_runtime_abort_offset(stock_dual_adc_runtime_t *p){p->calibrating=0u;p->offset_valid=0u;}
void stock_dual_adc_runtime_next_sector(stock_dual_adc_runtime_t *p,uint8_t sector) {
    if(sector>=1u && sector<=6u)p->sector=sector;
}
uint8_t stock_dual_adc_runtime_rotate(uint8_t sector){return sector==1u?2u:sector==2u?4u:1u;}
static void assign_sample(stock_dual_adc_runtime_t *p,uint8_t channel,uint16_t raw){
    if(channel<3u || channel>5u)return;
    const unsigned i=(unsigned)channel-3u;
    p->raw[i]=raw;
    if(p->calibrating && p->cal_samples[i]<STOCK_DUAL_OFFSET_SAMPLES){
        p->cal_sum[i]+=raw;p->cal_samples[i]++;
    }
}
uint8_t stock_dual_adc_runtime_capture(stock_dual_adc_runtime_t *p,uint16_t raw_adc1,uint16_t raw_adc2){
    const stock_adc_pair_t pair=stock_current_pair_for_sector(p->sector);
    if(pair.adc1_channel==pair.adc2_channel)return 0u;
    assign_sample(p,pair.adc1_channel,raw_adc1);
    assign_sample(p,pair.adc2_channel,raw_adc2);
    if(!p->calibrating)return 0u;
    for(unsigned i=0u;i<3u;i++)if(p->cal_samples[i]<STOCK_DUAL_OFFSET_SAMPLES)return 0u;
    for(unsigned i=0u;i<3u;i++)p->offsets[i]=(uint16_t)(p->cal_sum[i]/STOCK_DUAL_OFFSET_SAMPLES);
    p->calibrating=0u;p->offset_valid=1u;
    return 1u;
}
stock_phase_currents_t stock_dual_adc_runtime_currents(const stock_dual_adc_runtime_t *p,uint16_t raw_adc1,uint16_t raw_adc2) {
    return stock_current_reconstruct_counts(p->sector,raw_adc1,raw_adc2,p->offsets);
}
