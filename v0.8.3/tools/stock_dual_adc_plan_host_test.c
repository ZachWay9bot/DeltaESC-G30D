#include <stdint.h>
#include <stdio.h>
#include "stock_dual_adc_plan.h"

static int fail(int n){printf("FAIL %d\n",n);return n;}
int main(void){
    stock_dual_adc_plan_t p=stock_dual_adc_plan_init(2u);
    if(p.rcc_apb2_enable!=((1u<<9)|(1u<<10)))return fail(1);
    if((p.adc1_cr1&0x000F0000u)!=0x00010000u)return fail(2);
    if(!(p.adc1_cr1&(1u<<7))||p.adc2_cr1!=0u)return fail(3);
    if((p.adc1_cr2&(7u<<12))!=(1u<<12)||!(p.adc1_cr2&(1u<<15))||!(p.adc1_cr2&1u))return fail(4);
    if(p.adc2_cr2!=1u)return fail(5);
    if(p.adc1_jsqr!=0x18000u||p.adc2_jsqr!=0x28000u)return fail(6);
    static const uint8_t want[6][2]={{4,5},{3,5},{3,5},{3,4},{3,4},{4,5}};
    for(uint8_t s=1;s<=6;s++){
        stock_dual_adc_plan_next(&p,s);
        if(p.adc1_channel!=want[s-1][0]||p.adc2_channel!=want[s-1][1])return fail(10+s);
        if(p.adc1_jsqr!=stock_current_jsqr_one(want[s-1][0]))return fail(20+s);
        if(p.adc2_jsqr!=stock_current_jsqr_one(want[s-1][1]))return fail(30+s);
        if((p.adc1_cr2&(7u<<12))!=(1u<<12)||p.adc2_cr2!=1u)return fail(40+s);
    }
    puts("stock DRV126 dual-ADC acquisition plan: PASS");
    return 0;
}
