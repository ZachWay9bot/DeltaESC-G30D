#include "g30_passive_bemf_adc.h"
static const uint8_t channels[3]={6u,7u,9u};
void g30_passive_bemf_adc_init(g30_passive_bemf_adc_t *s,g30_passive_bemf_adc_io_t io,void *ctx){
    if(!s)return;
    *s=(g30_passive_bemf_adc_t){0};
    s->io=io;s->ctx=ctx;s->period_ms=3u;
}
void g30_passive_bemf_adc_cancel(g30_passive_bemf_adc_t *s){
    if(!s)return;
    if(s->pending&&s->io.abort_regular)s->io.abort_regular(s->ctx);
    s->pending=0u;s->fresh=0u;s->channel_index=0u;s->repeat_index=0u;
    s->sum[0]=s->sum[1]=s->sum[2]=0u;
}
void g30_passive_bemf_adc_tick(g30_passive_bemf_adc_t *s,uint32_t now_ms,uint8_t bridge_armed,uint8_t regular_adc_busy){
    if(!s||!s->io.start_regular||!s->io.poll_regular)return;
    if(bridge_armed){
        g30_passive_bemf_adc_cancel(s);s->next_start_ms=now_ms+s->period_ms;return;
    }
    if(s->pending){
        uint16_t v=0u;
        if(s->io.poll_regular(s->ctx,&v)){
            s->pending=0u;
            s->sum[s->channel_index]=(uint16_t)(s->sum[s->channel_index]+v);
            if(++s->repeat_index>=3u){
                s->repeat_index=0u;
                if(++s->channel_index>=3u){
                    for(unsigned i=0;i<3u;i++){
                        s->out[i]=(uint16_t)((s->sum[i]+1u)/3u);s->sum[i]=0u;
                    }
                    s->channel_index=0u;s->fresh=1u;
                    if(s->completed_frames<65535u)s->completed_frames++;
                    s->next_start_ms=now_ms+s->period_ms;
                }
            }
        }else if((uint32_t)(now_ms-s->started_ms)>2u){
            if(s->io.abort_regular)s->io.abort_regular(s->ctx);
            s->pending=0u;s->channel_index=0u;s->repeat_index=0u;
            s->sum[0]=s->sum[1]=s->sum[2]=0u;
            if(s->timeouts<65535u)s->timeouts++;
            s->next_start_ms=now_ms+s->period_ms;
        }
        return;
    }
    if(regular_adc_busy || (int32_t)(now_ms-s->next_start_ms)<0)return;
    if(s->io.start_regular(s->ctx,channels[s->channel_index])){
        s->pending=1u;s->started_ms=now_ms;
    }
}
uint8_t g30_passive_bemf_adc_take(g30_passive_bemf_adc_t *s,uint16_t out[3]){
    if(!s||!out||!s->fresh)return 0u;
    out[0]=s->out[0];out[1]=s->out[1];out[2]=s->out[2];s->fresh=0u;
    return 1u;
}
