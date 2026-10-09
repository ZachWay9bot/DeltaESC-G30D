#include "stock_adc_snapshot.h"
static void le16(uint8_t *p,uint16_t x){p[0]=(uint8_t)x;p[1]=(uint8_t)(x>>8);}
static void le32(uint8_t *p,uint32_t x){le16(p,(uint16_t)x);le16(p+2,(uint16_t)(x>>16));}
void stock_adc_snapshot_capture(stock_adc_snapshot_t *s,uint8_t sector,
                                uint16_t raw1,uint16_t raw2,uint32_t jsqr1,
                                uint32_t jsqr2,uint16_t sr1,uint16_t sr2,
                                uint32_t timestamp_ms){
    if(!s)return;
    s->sector=sector;s->adc1_raw=raw1;s->adc2_raw=raw2;
    s->adc1_jsqr=jsqr1;s->adc2_jsqr=jsqr2;
    s->adc1_sr=sr1;s->adc2_sr=sr2;s->timestamp_ms=timestamp_ms;
}
void stock_adc_snapshot_encode_ea(const stock_adc_snapshot_t *s,uint8_t out[16]){
    if(!out)return;
    if(!s){for(unsigned i=0;i<16u;i++)out[i]=0u;return;}
    le32(out,s->adc1_jsqr);le32(out+4,s->adc2_jsqr);
    le16(out+8,s->adc1_sr);le16(out+10,s->adc2_sr);
    le16(out+12,s->adc1_raw);le16(out+14,s->adc2_raw);
}
