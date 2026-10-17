#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "g30_rolling_bemf.h"
#define T(x) do{if(!(x)){fprintf(stderr,"FAIL %s:%d %s\n",__FILE__,__LINE__,#x);return 1;}}while(0)
static uint16_t adiff(uint16_t a,uint16_t b){int16_t d=(int16_t)(a-b);return (uint16_t)(d<0?-(int32_t)d:(int32_t)d);}
static void abc_for_sector(uint8_t s,uint16_t *a,uint16_t *b,uint16_t *c){
    static const uint16_t v[6][3]={{3000,2000,1000},{2000,3000,1000},{1000,3000,2000},{1000,2000,3000},{2000,1000,3000},{3000,1000,2000}};
    *a=v[s][0];*b=v[s][1];*c=v[s][2];
}
int main(void){
    g30_rolling_bemf_t m;g30_rolling_bemf_config_t c;g30_rolling_bemf_default_config(&c);g30_rolling_bemf_init(&m,&c);
    uint32_t now=1000000u,dt=400000u; /* ~4.9 km/h with FO70 geometry */
    uint8_t seq[]={0,5,4,3,2,1,0};
    for(unsigned i=0;i<sizeof(seq);i++){uint16_t a,b,d;abc_for_sector(seq[i],&a,&b,&d);g30_rolling_bemf_sample(&m,a,b,d,now);now+=dt;}
    T(m.direction_forward);T(m.forward_edges>=4u);T(m.electrical_step_u16<0);T(g30_rolling_bemf_ready(&m,now-dt+1000u));
    /* sector 0 sample 3000/2000/1000 => e ~= +30deg; negative-forward PM flux = e+90 ~=120deg */
    T(adiff(m.phase_u16,21845u)<1800u);
    T(g30_rolling_bemf_speed_kmh10(&m,now-dt+1000u)>=30u);
    {uint16_t a,b,d;abc_for_sector(1u,&a,&b,&d);g30_rolling_bemf_sample(&m,a,b,d,now);}
    T(!g30_rolling_bemf_ready(&m,now));T(!m.direction_forward);T(m.reverse_edges>0u);
    g30_rolling_bemf_reset(&m);g30_rolling_bemf_sample(&m,2020,2000,1990,now);T(m.last_sector==G30_ROLL_NO_SECTOR);T(!m.forward_edges);
    puts("PASS G30 rolling BEMF: FO70 sector order, BEMF->PM-flux phase, forward direction, speed gate, stale/reverse fail-closed");
    return 0;
}
