#include "g30_bms.h"

static void count(uint16_t *v) { if(*v!=65535u)++*v; }
static uint16_t checksum(const uint8_t *p,uint8_t n) {
    uint16_t s=0u;
    for(uint8_t i=0u;i<n;i++)s=(uint16_t)(s+p[i]);
    return (uint16_t)~s;
}
void g30_bms_init(g30_bms_t *b) { if(b)*b=(g30_bms_t){0}; }
void g30_bms_reset_parser(g30_bms_t *b) { if(b)b->pos=0u; }
uint8_t g30_bms_read_soc_request(uint8_t out[10]) {
    if(!out)return 0u;
    const uint8_t header[8]={0x5a,0xa5,1,0x20,0x22,1,0x32,2};
    for(unsigned i=0;i<8u;i++)out[i]=header[i];
    uint16_t ck=checksum(out+2,6u);
    out[8]=(uint8_t)ck;out[9]=(uint8_t)(ck>>8);
    return 10u;
}
void g30_bms_feed(g30_bms_t *b,uint8_t v,uint32_t now) {
    if(!b)return;
    if(b->pos && (uint32_t)(now-b->last_byte_ms)>G30_BMS_BYTE_TIMEOUT_MS)
        b->pos=0u;
    b->last_byte_ms=now;
    if(!b->pos) {
        if(v==0x5au){b->raw[0]=v;b->pos=1u;}
        return;
    }
    if(b->pos==1u) {
        if(v==0xa5u){b->raw[1]=v;b->pos=2u;}
        else if(v!=0x5au)b->pos=0u;
        return;
    }
    /* This reader supports exactly one 16-bit SOC register response. */
    if(b->pos==2u && v!=2u) {
        count(&b->rejected);b->pos=(v==0x5au)?1u:0u;return;
    }
    b->raw[b->pos++]=v;
    if(b->pos!=sizeof(b->raw))return;
    b->pos=0u;
    const uint8_t *r=b->raw;
    const uint16_t got=(uint16_t)r[9]|((uint16_t)r[10]<<8);
    const uint16_t soc=(uint16_t)r[7]|((uint16_t)r[8]<<8);
    if(got!=checksum(r+2,7u) || r[3]!=0x22u || r[4]!=0x20u ||
       r[5]!=0x04u || r[6]!=0x32u || soc>100u) {
        count(&b->rejected);return;
    }
    b->soc=(uint8_t)soc;b->seen=1u;b->last_soc_ms=now;
    count(&b->accepted);
}
uint8_t g30_bms_soc_valid(const g30_bms_t *b,uint32_t now) {
    return (uint8_t)(b && b->seen &&
                    (uint32_t)(now-b->last_soc_ms)<=G30_BMS_SOC_TIMEOUT_MS);
}
uint8_t g30_bms_soc(const g30_bms_t *b,uint32_t now) {
    return g30_bms_soc_valid(b,now)?b->soc:0u;
}
void g30_bms_status_payload(const g30_bms_t *b,uint32_t now,uint8_t out[16]) {
    if(!out)return;
    for(unsigned i=0;i<16u;i++)out[i]=0u;
    out[0]='B';out[1]='M';out[2]='1';out[3]='3';
    if(!b)return;
    out[4]=(uint8_t)(g30_bms_soc_valid(b,now)|(b->seen?2u:0u));
    out[5]=g30_bms_soc(b,now);
    out[6]=(uint8_t)b->accepted;out[7]=(uint8_t)(b->accepted>>8);
    uint32_t age=b->seen?(uint32_t)(now-b->last_soc_ms):0xffffffffu;
    for(unsigned i=0;i<4u;i++)out[8u+i]=(uint8_t)(age>>(8u*i));
    out[12]=(uint8_t)b->rejected;out[13]=(uint8_t)(b->rejected>>8);
    /* bytes 14/15 are supplied by the separate UART driver */
}
