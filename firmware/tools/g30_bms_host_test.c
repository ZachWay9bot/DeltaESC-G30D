#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "g30_bms.h"

static const uint8_t soc67[]={0x5a,0xa5,2,0x22,0x20,4,0x32,67,0,0x42,0xff};
static void feed(g30_bms_t *b,const uint8_t *p,unsigned n,uint32_t now) {
    for(unsigned i=0;i<n;i++)g30_bms_feed(b,p[i],now);
}
static void repair(uint8_t p[11]) {
    unsigned sum=0;
    for(unsigned i=2;i<9;i++)sum+=p[i];
    unsigned crc=0xffffu-sum;p[9]=(uint8_t)crc;p[10]=(uint8_t)(crc>>8);
}
int main(void) {
    g30_bms_t b;g30_bms_init(&b);
    uint8_t out[16],rq[10];
    const uint8_t want[]={0x5a,0xa5,1,0x20,0x22,1,0x32,2,0x87,0xff};
    assert(g30_bms_read_soc_request(rq)==10u && !memcmp(rq,want,10));
    assert(!g30_bms_soc_valid(&b,0) && g30_bms_soc(&b,0)==0);
    g30_bms_status_payload(&b,0,out);
    assert(!memcmp(out,"BM13",4) && !out[4] && out[8]==255u && out[11]==255u);
    feed(&b,soc67,sizeof soc67,0);
    assert(g30_bms_soc_valid(&b,0) && g30_bms_soc(&b,0)==67u);
    assert(g30_bms_soc_valid(&b,2000u));
    assert(!g30_bms_soc_valid(&b,2001u) && !g30_bms_soc(&b,2001u));
    g30_bms_status_payload(&b,2001u,out);
    assert(out[4]==2u && !out[5] && out[6]==1u);

    /* Every single-bit corruption of the complete wire message is rejected. */
    for(unsigned i=0;i<sizeof soc67;i++)for(unsigned bit=0;bit<8;bit++) {
        uint8_t bad[sizeof soc67];memcpy(bad,soc67,sizeof bad);bad[i]^=(uint8_t)(1u<<bit);
        g30_bms_init(&b);feed(&b,bad,sizeof bad,1);
        assert(!g30_bms_soc_valid(&b,1));
        g30_bms_reset_parser(&b);feed(&b,soc67,sizeof soc67,2);
        assert(g30_bms_soc(&b,2)==67u);
    }
    /* A checksum-valid write echo, dashboard frame, unknown register or bad
     * SOC cannot masquerade as a fresh battery observation. */
    const unsigned index[]={3,4,5,6,7,8};
    const uint8_t value[]={0x21,0x21,2,0x34,101,1};
    for(unsigned k=0;k<sizeof index/sizeof index[0];k++) {
        uint8_t bad[11];memcpy(bad,soc67,sizeof bad);bad[index[k]]=value[k];repair(bad);
        g30_bms_init(&b);feed(&b,soc67,sizeof soc67,100);
        feed(&b,bad,sizeof bad,2100);
        assert(b.last_soc_ms==100 && !g30_bms_soc_valid(&b,2101));
    }
    for(unsigned soc=0;soc<=100;soc++) {
        uint8_t p[11];memcpy(p,soc67,sizeof p);p[7]=(uint8_t)soc;repair(p);
        g30_bms_init(&b);feed(&b,p,sizeof p,50);
        assert(g30_bms_soc_valid(&b,50) && g30_bms_soc(&b,50)==soc);
    }
    /* All length-byte values: only an exact two-byte response is supported. */
    for(unsigned len=0;len<=255;len++) {
        uint8_t p[11];memcpy(p,soc67,sizeof p);p[2]=(uint8_t)len;repair(p);
        g30_bms_init(&b);feed(&b,p,sizeof p,1);
        assert(g30_bms_soc_valid(&b,1)==(len==2));
    }
    g30_bms_init(&b);feed(&b,soc67,5,10);
    feed(&b,soc67+5,sizeof soc67-5,36); /* partial packet expired */
    assert(!g30_bms_soc_valid(&b,36));
    feed(&b,soc67,sizeof soc67,37);assert(g30_bms_soc(&b,37)==67);
    g30_bms_init(&b);g30_bms_feed(&b,0x5a,0);
    feed(&b,soc67,sizeof soc67,1);assert(g30_bms_soc(&b,1)==67);

    /* Freshness works across the 32-bit millisecond counter wrap. */
    g30_bms_init(&b);feed(&b,soc67,sizeof soc67,0xfffffff0u);
    assert(g30_bms_soc_valid(&b,0xfffffff0u+2000u));
    assert(!g30_bms_soc_valid(&b,0xfffffff0u+2001u));
    b.accepted=65535u;feed(&b,soc67,sizeof soc67,42);assert(b.accepted==65535u);
    b.rejected=65535u;uint8_t bad[11];memcpy(bad,soc67,sizeof bad);bad[9]^=1;
    feed(&b,bad,sizeof bad,43);assert(b.rejected==65535u);
    puts("PASS stock BMS: wire request, all SOC/length values, corruption, source/command checks, stale/partial/wrap and saturation");
    return 0;
}
