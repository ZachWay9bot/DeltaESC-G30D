#include <stdint.h>
#include <stdio.h>
#include "stock_adc_timing.h"
#define T(x) do{if(!(x)){printf("FAIL timing line %d: %s\n",__LINE__,#x);return 1;}}while(0)
static uint16_t u16(const uint8_t*p){return (uint16_t)(p[0]|((uint16_t)p[1]<<8));}
static uint32_t u32(const uint8_t*p){return (uint32_t)u16(p)|((uint32_t)u16(p+2)<<16);}
int main(void){
    stock_adc_timing_t s={0};uint8_t eb[16]={0},ec[16]={0};
    T(!stock_adc_timing_flags(1999,1800,0xA1,0x1000,0x8040));
    T(stock_adc_timing_flags(1999,1800,0xA0,0x1000,0x8040)==ADC_TIMING_NO_CLOCK);
    T(stock_adc_timing_flags(1999,1800,0x81,0x1000,0x8040)==ADC_TIMING_NOT_CENTER);
    T(stock_adc_timing_flags(1999,1800,0xA1,0x0000,0x8040)==ADC_TIMING_NO_CC4);
    T(stock_adc_timing_flags(1999,1800,0xA1,0x1001,0x8040)==ADC_TIMING_GATE_ENABLED);
    T(stock_adc_timing_flags(1999,2000,0xA1,0x1000,0x8040)==ADC_TIMING_BAD_COMPARE);
    T(stock_adc_timing_flags(0,0,0xA1,0x1000,0x8040)==ADC_TIMING_BAD_COMPARE);
    T(stock_adc_timing_flags(1999,1800,0xA1,0x1000,0x0040)==ADC_TIMING_NO_MOE);
    stock_adc_timing_capture(&s,1802,1800,1999,0xA1,0x1000,0x8040,0x0010,0xffffff00u);
    T(s.irq_period_cycles==0 && s.sample_sequence==1 && s.config_flags==0);
    stock_adc_timing_encode_eb(&s,eb);stock_adc_timing_encode_ec(&s,ec);
    T(u16(eb)==1802 && u16(eb+2)==1800 && u16(eb+4)==1999);
    T(u16(eb+6)==0xA1 && u16(eb+8)==0x1000 && u16(eb+10)==0x8040);
    T(u32(eb+12)==0xffffff00u);
    T(u32(ec)==1u && u32(ec+4)==0u && u16(ec+8)==0 && u16(ec+10)==0 && u16(ec+12)==0x10);
    stock_adc_timing_capture(&s,1803,1800,1999,0xA1,0x1000,0x8040,0x0010,0x00000eA0u);
    T(s.irq_period_cycles==4000u && s.sample_sequence==2);
    stock_adc_timing_capture(&s,1900,1800,1999,0xA1,0x1001,0x8040,0x0010,0x00001e40u);
    T(s.config_flags==ADC_TIMING_GATE_ENABLED && s.fault_count==1 && s.sample_sequence==3);
    stock_adc_timing_encode_ec(&s,ec);
    T(u32(ec)==3 && u32(ec+4)==4000 && u16(ec+8)==ADC_TIMING_GATE_ENABLED && u16(ec+10)==1);
    s.fault_count=65535;
    stock_adc_timing_capture(&s,100,1800,1999,0xA1,0x1001,0x8040,0,0x2e00);
    T(s.fault_count==65535);
    stock_adc_timing_encode_eb(0,eb);stock_adc_timing_encode_ec(0,ec);
    for(int i=0;i<16;i++)T(!eb[i]&&!ec[i]);
    puts("PASS: ADC/TIM1 timing evidence EB/EC, wraparound, six gate/config checks, saturated counters");
    return 0;
}
