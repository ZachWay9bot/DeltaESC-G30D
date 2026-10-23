#include <stdint.h>
#include <stdio.h>
#include "stock_adc_mode_guard.h"
#include "stock_dual_adc_plan.h"
#define T(c) do{if(!(c)){printf("FAIL line %d: %s\n",__LINE__,#c);return 1;}}while(0)
static uint16_t u16(const uint8_t*p){return (uint16_t)p[0]|((uint16_t)p[1]<<8);}
static uint32_t u32(const uint8_t*p){return (uint32_t)u16(p)|((uint32_t)u16(p+2)<<16);}
int main(void){
    g30_adc_mode_guard_t s={0};uint8_t p[16];
    T(G30_ADC_DUALMOD_INJECTED_ONLY==0x00050000u);
    T(G30_ADC_DUALMOD_MASK==0x000f0000u);
    T(STOCK_ADC_MODE_REG_INJEC_SIMULT==G30_ADC_DUALMOD_INJECTED_ONLY);
    stock_dual_adc_plan_t plan=stock_dual_adc_plan_init(1);
    T((plan.adc1_cr1&G30_ADC_DUALMOD_MASK)==G30_ADC_DUALMOD_INJECTED_ONLY);
    T(plan.adc2_cr1==0u);
    T(g30_adc_mode_flags(plan.adc1_cr1,plan.adc2_cr1,G30_ADC_JEOC)==0u);
    T((g30_adc_mode_flags(0x00010000u,0,G30_ADC_JEOC)&G30_ADCMODE_BAD_MASTER)!=0u);
    T((g30_adc_mode_flags(plan.adc1_cr1,0x00010000u,G30_ADC_JEOC)&G30_ADCMODE_BAD_SLAVE)!=0u);
    T(g30_adc_mode_flags(plan.adc1_cr1,0,0)==G30_ADCMODE_ADC2_NOT_READY);
    T(g30_adc_mode_capture(&s,plan.adc1_cr1,0,0x8001u,G30_ADC_JEOC)==0u);
    T(s.fault_count==0 && !s.ever_faulted);
    T(g30_adc_mode_capture(&s,0x00010000u,0,0x8001u,0)==(G30_ADCMODE_BAD_MASTER|G30_ADCMODE_ADC2_NOT_READY));
    T(s.ever_faulted && s.fault_count==1);
    g30_adc_mode_encode_ee(&s,p);
    T(u32(p)==0x00010000u&&u32(p+4)==0u&&u32(p+8)==0x8001u);
    T(u16(p+12)==1 && p[14]==(G30_ADCMODE_BAD_MASTER|G30_ADCMODE_ADC2_NOT_READY)&&p[15]==1);
    s.fault_count=65535u;
    g30_adc_mode_capture(&s,0x00010000u,0,0,0);
    T(s.fault_count==65535u);
    g30_adc_mode_encode_ee(NULL,p);
    for(unsigned i=0;i<16;i++)T(p[i]==0);
    T(g30_adc_mode_capture(NULL,0,0,0,0)==7u);
    puts("PASS STM32F103 ADC DUALMOD 0101, independent regular VBUS, ADC2 JEOC guard, EE little-endian and saturation");
    return 0;
}
