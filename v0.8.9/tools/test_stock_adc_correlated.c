#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "stock_adc_correlated.h"
#include "stock_current_frontend.h"
#include "stock_adc_quality.h"
#define CHECK(x) do { if(!(x)){printf("FAIL line %d: %s\n",__LINE__,#x);return 1;} }while(0)
static uint16_t u16(const uint8_t *p){return (uint16_t)p[0]|((uint16_t)p[1]<<8);}
static uint32_t u32(const uint8_t *p){return (uint32_t)u16(p)|((uint32_t)u16(p+2)<<16);}
int main(void){
    stock_adc_correlated_t record={0};
    uint8_t out[16]={0};
    stock_adc_timing_t timer={0};
    stock_adc_snapshot_t adc={0};
    stock_adc_correlated_encode_ed(&record,out);
    CHECK(u32(out)==0u);
    for(uint8_t sector=1u;sector<=6u;sector++){
        const stock_adc_pair_t pair=stock_current_pair_for_sector(sector);
        stock_adc_timing_capture(&timer,1799u+sector,1800u,1999u,
                                  0xA1u,0x1000u,0x8040u,0x0010u,3000u+4000u*sector);
        stock_adc_snapshot_capture(&adc,sector,1500u+sector,2000u+sector,
                                  stock_current_jsqr_one(pair.adc1_channel),
                                  stock_current_jsqr_one(pair.adc2_channel),
                                  (1u<<2),(1u<<2),sector*4u);
        stock_adc_correlated_capture(&record,&adc,&timer,0u);
        /* Simulate a NEXT-sector JSQR update: old correlated record is immutable. */
        const stock_adc_pair_t next=stock_current_pair_for_sector((sector%6u)+1u);
        adc.adc2_jsqr=stock_current_jsqr_one(next.adc2_channel);
        stock_adc_correlated_encode_ed(&record,out);
        CHECK(u32(out)==sector);
        CHECK(u16(out+4)==1500u+sector&&u16(out+6)==2000u+sector);
        CHECK(u16(out+8)==1799u+sector&&u16(out+10)==1800u);
        CHECK(out[12]==sector);
        CHECK(out[13]==pair.adc1_channel&&out[14]==pair.adc2_channel);
        CHECK(out[15]==(ADC_ED_ADC1_JEOC|ADC_ED_ADC2_JEOC));
    }
    /* Bad reading is recorded for offline diagnosis, never marked valid. */
    adc.adc2_sr=0u;
    stock_adc_correlated_capture(&record,&adc,&timer,ADCQ_RAW_INVALID);
    stock_adc_correlated_encode_ed(&record,out);
    CHECK((out[15]&ADC_ED_BAD_PAIR)!=0);
    CHECK((out[15]&ADC_ED_ADC2_JEOC)==0);
    timer.config_flags=ADC_TIMING_GATE_ENABLED|ADC_TIMING_BAD_COMPARE;
    stock_adc_correlated_capture(&record,&adc,&timer,ADCQ_GAP_LONG);
    stock_adc_correlated_encode_ed(&record,out);
    CHECK((out[15]&(ADC_ED_GATE_ENABLED|ADC_ED_INTERVAL_ANOMALY|
            ADC_ED_TIM1_CONFIG_ANOMALY))==
            (ADC_ED_GATE_ENABLED|ADC_ED_INTERVAL_ANOMALY|
             ADC_ED_TIM1_CONFIG_ANOMALY));
    stock_adc_correlated_encode_ed(NULL,out);
    for(unsigned i=0;i<16u;i++)CHECK(out[i]==0u);
    stock_adc_correlated_capture(NULL,&adc,&timer,0);
    stock_adc_correlated_capture(&record,NULL,&timer,0);
    stock_adc_correlated_capture(&record,&adc,NULL,0);
    puts("PASS: ED combines six exact ADC sector pairs and TIM1 from same ISR; rejects stale next-sector JSQR; fault flags");
    return 0;
}
