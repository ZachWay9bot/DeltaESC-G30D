#include <stdio.h>
#include <stdint.h>
#include "g30_fixed_scan.h"
#define CHECK(x) do {if(!(x)){printf("FAIL fixed scan line %d: %s\n",__LINE__,#x);return 1;}}while(0)
static uint16_t le16(const uint8_t*p){return (uint16_t)p[0]|((uint16_t)p[1]<<8);}
int main(void){
    g30_fixed_scan_t s;uint16_t initial[3]={2000,2010,2020};uint8_t out[16];
    g30_fixed_scan_init(&s,initial);
    CHECK(G30_FIXED_JSQR_A==((1u<<20)|(3u<<10)|(4u<<15)));
    CHECK(G30_FIXED_JSQR_B==((1u<<20)|(5u<<10)|(5u<<15)));
    CHECK(((G30_FIXED_JSQR_A>>20)&3u)==1u);
    CHECK(((G30_FIXED_JSQR_A>>10)&31u)==3u&&((G30_FIXED_JSQR_A>>15)&31u)==4u);
    CHECK(((G30_FIXED_JSQR_B>>10)&31u)==5u&&((G30_FIXED_JSQR_B>>15)&31u)==5u);
    CHECK(g30_fixed_scan_capture(&s,G30_FIXED_JSQR_A,G30_FIXED_JSQR_B,0x00050100u,0x100u,4,4,2020,2030,2040,2045)==0u);
    CHECK(s.samples==1&&s.raw[0]==2020&&s.raw[1]==2030&&s.raw[2]==2040);
    g30_fixed_scan_start_offsets(&s);
    for(unsigned i=0;i<512;i++)CHECK(g30_fixed_scan_capture(&s,G30_FIXED_JSQR_A,G30_FIXED_JSQR_B,0x00050100u,0x100u,4,4,2100,2200,2300,2302)==0u);
    CHECK(s.offset_valid&&s.cal_count==512&&!s.calibrating);
    CHECK(s.offsets[0]==2100&&s.offsets[1]==2200&&s.offsets[2]==2300);
    g30_fixed_scan_encode_ef(&s,out);
    CHECK(le16(out)==2100&&le16(out+2)==2200&&le16(out+4)==2300);
    CHECK(le16(out+6)==2100&&le16(out+8)==2200&&le16(out+10)==2300);
    CHECK(le16(out+12)==2302&&out[14]==0&&out[15]==2);
    CHECK(g30_fixed_scan_capture(&s,G30_FIXED_JSQR_A,G30_FIXED_JSQR_B,0x00050100u,0x100u,4,4,2048,2050,2060,2300)==G30_SCAN_DUP_SPREAD);
    CHECK(s.offset_valid&&s.bad_count==0);
    CHECK(g30_fixed_scan_capture(&s,G30_FIXED_JSQR_A,0,0x00050100u,0x100u,4,4,2048,2050,2060,2060)&G30_SCAN_BAD_JSQR);
    CHECK(!s.offset_valid&&s.bad_count==1);
    CHECK(g30_fixed_scan_capture(&s,G30_FIXED_JSQR_A,G30_FIXED_JSQR_B,0x00050100u,0x100u,4,0,2048,2050,2060,2060)&G30_SCAN_NO_JEOC);
    CHECK(g30_fixed_scan_capture(&s,G30_FIXED_JSQR_A,G30_FIXED_JSQR_B,0x00050100u,0x100u,4,4,4096,2050,2060,2060)&G30_SCAN_BAD_RAW);
    CHECK(g30_fixed_scan_capture(&s,G30_FIXED_JSQR_A,G30_FIXED_JSQR_B,0x00010100u,0x100u,4,4,2000,2050,2060,2060)&G30_SCAN_BAD_MODE);
    /* Duplicated CH5 ranks may be separated by actual ADC time and cannot
       be treated as the same current without quality qualification. */
    g30_fixed_scan_start_offsets(&s);
    CHECK(g30_fixed_scan_capture(&s,G30_FIXED_JSQR_A,G30_FIXED_JSQR_B,
        0x00050100u,0x100u,4,4,2100,2200,2300,2333)&G30_SCAN_CAL_UNSTABLE);
    CHECK(!s.offset_valid && !s.calibrating && s.cal_count==0u);
    g30_fixed_scan_encode_f6(&s,out);
    CHECK(le16(out+12)==33u && le16(out+14)==0u);
    g30_fixed_scan_start_offsets(&s);
    CHECK(g30_fixed_scan_capture(&s,G30_FIXED_JSQR_A,G30_FIXED_JSQR_B,
        0x00050100u,0x100u,4,4,2100,2200,2300,2302)==0);
    CHECK(g30_fixed_scan_capture(&s,G30_FIXED_JSQR_A,G30_FIXED_JSQR_B,
        0x00050100u,0x100u,4,4,2165,2200,2300,2302)&G30_SCAN_CAL_UNSTABLE);
    CHECK(!s.offset_valid && !s.calibrating && s.cal_count==1u);
    g30_fixed_scan_encode_f6(&s,out);
    CHECK(le16(out)==2100u && le16(out+6)==2165u);
    g30_fixed_scan_start_offsets(&s);
    for(unsigned k=0;k<G30_FIXED_OFFSET_SAMPLES;k++){
        const uint16_t a=(uint16_t)(2100u+(k%7u));
        const uint16_t b=(uint16_t)(2200u+(k%9u));
        const uint16_t c=(uint16_t)(2300u+(k%11u));
        CHECK(g30_fixed_scan_capture(&s,G30_FIXED_JSQR_A,G30_FIXED_JSQR_B,
            0x00050100u,0x100u,4,4,a,b,c,(uint16_t)(c+3u))==0u);
    }
    CHECK(s.offset_valid && s.cal_count==G30_FIXED_OFFSET_SAMPLES);
    g30_fixed_scan_encode_f6(&s,out);
    CHECK(le16(out)==2100u && le16(out+6)==2106u);
    CHECK(le16(out+12)==3u && le16(out+14)==512u);
    g30_fixed_scan_encode_f6(NULL,out);
    for(unsigned i=0u;i<16u;i++)CHECK(out[i]==0u);
    g30_fixed_scan_encode_ef(0,out);for(unsigned i=0;i<16;i++)CHECK(out[i]==0);
    puts("PASS: STM32F103 static two-rank dual ADC, three distinct raw channels, 512 sample offsets and fail-closed EF");
    return 0;
}
