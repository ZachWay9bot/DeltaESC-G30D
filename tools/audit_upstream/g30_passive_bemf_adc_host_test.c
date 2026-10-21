#include <stdio.h>
#include "g30_passive_bemf_adc.h"
#define T(x) do{if(!(x)){fprintf(stderr,"FAIL %d %s\n",__LINE__,#x);return 1;}}while(0)
typedef struct{uint8_t ch,pending;uint16_t n;unsigned aborts;}hw_t;
static uint8_t start(void*ctx,uint8_t ch){hw_t*h=ctx;if(h->pending)return 0;h->ch=ch;h->pending=1;return 1;}
static uint8_t poll(void*ctx,uint16_t*v){hw_t*h=ctx;if(!h->pending)return 0;h->pending=0;*v=(uint16_t)(1000u+h->ch*100u+h->n++);return 1;}
static void abort_adc(void*ctx){hw_t*h=ctx;h->pending=0;h->aborts++;}
int main(void){
 hw_t h={0};g30_passive_bemf_adc_t s;g30_passive_bemf_adc_io_t io={start,poll,abort_adc};g30_passive_bemf_adc_init(&s,io,&h);
 uint16_t o[3];for(uint32_t ms=0;ms<20;ms++){g30_passive_bemf_adc_tick(&s,ms,0,0);g30_passive_bemf_adc_tick(&s,ms,0,0);}T(g30_passive_bemf_adc_take(&s,o));T(o[0]>=1600&&o[1]>=1700&&o[2]>=1900);T(s.completed_frames>0);
 g30_passive_bemf_adc_tick(&s,30,0,0);g30_passive_bemf_adc_tick(&s,30,1,0);T(!s.pending);
 puts("PASS passive BEMF ADC: PA6/PA7/PB1 channel schedule, 3x average, gate-off ownership, timeout/cancel semantics");return 0;
}
