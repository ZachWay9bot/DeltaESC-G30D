#include <stdint.h>
#include <stdio.h>
#include "stock_current_frontend.h"

static int bad(int n,const char *s){printf("FAIL %d: %s\n",n,s);return n;}
int main(void){
    stock_adc_pair_t p;
    p=stock_current_pair_for_sector(1); if(p.adc1_channel!=4||p.adc2_channel!=5)return bad(1,"S1 pair");
    p=stock_current_pair_for_sector(6); if(p.adc1_channel!=4||p.adc2_channel!=5)return bad(2,"S6 pair");
    p=stock_current_pair_for_sector(2); if(p.adc1_channel!=3||p.adc2_channel!=5)return bad(3,"S2 pair");
    p=stock_current_pair_for_sector(3); if(p.adc1_channel!=3||p.adc2_channel!=5)return bad(4,"S3 pair");
    p=stock_current_pair_for_sector(4); if(p.adc1_channel!=3||p.adc2_channel!=4)return bad(5,"S4 pair");
    p=stock_current_pair_for_sector(5); if(p.adc1_channel!=3||p.adc2_channel!=4)return bad(6,"S5 pair");
    if(stock_current_jsqr_one(3)!=0x18000u||stock_current_jsqr_one(4)!=0x20000u||stock_current_jsqr_one(5)!=0x28000u)return bad(7,"JSQR");
    uint16_t o[3]={2048,2050,2046}; stock_phase_currents_t c;
    c=stock_current_reconstruct_counts(1,2060,2036,o);
    if(c.ia!=0||c.ib!=-10||c.ic!=10)return bad(8,"S1 reconstruction");
    c=stock_current_reconstruct_counts(2,2068,2036,o);
    if(c.ia!=-20||c.ib!=10||c.ic!=10)return bad(9,"S2 reconstruction");
    c=stock_current_reconstruct_counts(4,2068,2060,o);
    if(c.ia!=-20||c.ib!=-10||c.ic!=30)return bad(10,"S4 reconstruction");
    for(int s=1;s<=6;s++){
        p=stock_current_pair_for_sector((uint8_t)s);
        uint16_t r1=(uint16_t)(o[p.adc1_channel-3]+13);
        uint16_t r2=(uint16_t)(o[p.adc2_channel-3]-7);
        c=stock_current_reconstruct_counts((uint8_t)s,r1,r2,o);
        if((int)c.ia+(int)c.ib+(int)c.ic!=0)return bad(11+s,"sum");
    }
    if(stock_current_scaled_unit_from_delta(1)!=50)return bad(20,"scale1");
    if(stock_current_scaled_unit_from_delta(100)!=5036)return bad(21,"scale100");
    if(stock_current_scaled_unit_from_delta(-100)!=-5037)return bad(22,"scale-100");
    unsigned seen=0; const int32_t v[6][2]={{1000,1000},{0,1000},{-1000,1000},{-1000,-1000},{0,-1000},{1000,-1000}};
    for(unsigned i=0;i<6;i++){uint8_t s=stock_current_sector_from_ab(v[i][0],v[i][1]);if(s<1||s>6)return bad(30+i,"sector");seen|=1u<<s;}
    if((seen&0x7e)!=0x7e)return bad(40,"coverage");
    puts("stock DRV126 current frontend host test: PASS");return 0;
}
