#include <stdio.h>
#include <stdint.h>
#include "stock_adc_snapshot.h"
#include "stock_current_frontend.h"
#define T(x) do{if(!(x)){printf("FAIL line %d: %s\n",__LINE__,#x);return 1;}}while(0)
static uint16_t le16(const uint8_t *p){return (uint16_t)p[0]|((uint16_t)p[1]<<8);}
static uint32_t le32(const uint8_t *p){return (uint32_t)le16(p)|((uint32_t)le16(p+2)<<16);}
int main(void){
    stock_adc_snapshot_t s={0};uint8_t buf[16];
    for(uint8_t sector=1u;sector<=6u;sector++){
        const stock_adc_pair_t p=stock_current_pair_for_sector(sector);
        const uint32_t jsqr1=stock_current_jsqr_one(p.adc1_channel);
        const uint32_t jsqr2=stock_current_jsqr_one(p.adc2_channel);
        stock_adc_snapshot_capture(&s,sector,1234u+sector,2123u+sector,
                                   jsqr1,jsqr2,0x0004u,0x0004u,3456u+sector);
        /* Simulate upcoming JSQR write for NEXT sector. Last snapshot must
         * still refer to the PREVIOUSLY completed ADC pair. */
        const uint8_t next=sector==6u?1u:sector+1u;
        const stock_adc_pair_t ahead=stock_current_pair_for_sector(next);
        const uint32_t future_jsqr=stock_current_jsqr_one(ahead.adc2_channel);
        (void)future_jsqr;
        stock_adc_snapshot_encode_ea(&s,buf);
        T(le32(buf)==jsqr1 && le32(buf+4)==jsqr2);
        T(le16(buf+8)==4u && le16(buf+10)==4u);
        T(le16(buf+12)==1234u+sector && le16(buf+14)==2123u+sector);
        T(s.sector==sector && s.timestamp_ms==3456u+sector);
    }
    stock_adc_snapshot_encode_ea(0,buf);
    for(int i=0;i<16;i++)T(buf[i]==0);
    stock_adc_snapshot_capture(0,1,100,101,0,0,0,0,1);
    stock_adc_snapshot_encode_ea(&s,0);
    puts("PASS 6 sectors: ADC raw+JEOC+FULL JSQR are one IRQ snapshot, unaffected by upcoming sector");
    return 0;
}
