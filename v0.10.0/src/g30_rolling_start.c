#include "g30_rolling_start.h"

static uint16_t abs16s(int16_t x){return (uint16_t)(x<0?-(int32_t)x:(int32_t)x);}
static int16_t phase_delta(uint16_t to,uint16_t from){return (int16_t)(to-from);}

void g30_rolling_start_default_config(g30_rolling_start_config_t *c){
    if(!c)return;
    c->throttle_idle_max=3u;
    c->verify_ticks=320u;
    c->verify_timeout_ticks=1200u;
    c->ramp_ticks=1000u;
    c->target_iq_counts=12u;
    c->max_now_phase_error=8192u;
    c->max_avg_phase_error=5461u;
    c->max_abs_phase_step=1024u;
    c->max_positive_phase_step=256u;
}

void g30_rolling_start_init(g30_rolling_start_t *s){g30_rolling_start_reset(s);}
void g30_rolling_start_reset(g30_rolling_start_t *s){
    if(!s)return;
    *s=(g30_rolling_start_t){0};
    s->state=G30_RS_WAIT_NEUTRAL;
}

static void fail(g30_rolling_start_t *s,uint8_t reason){
    s->state=G30_RS_FAULT;s->abort_reason=reason;s->closed_loop_allowed=0u;
    s->iq_ref_counts=0u;s->seed_requested=0u;
}

static uint8_t common_inputs_ok(const g30_rolling_start_input_t *in){
    return (uint8_t)(in && in->dash_valid && !in->brake && !in->fault && in->motor_params_valid);
}

void g30_rolling_start_step(g30_rolling_start_t *s,const g30_rolling_start_config_t *c,
                            const g30_rolling_start_input_t *in){
    if(!s||!c||!in)return;
    s->seed_requested=0u;
    if(s->state==G30_RS_FAULT)return;
    if(!common_inputs_ok(in)){
        if(s->state>=G30_RS_VERIFY)fail(s,G30_RS_ABORT_INPUT);
        else {s->neutral_seen=0u;s->state=G30_RS_WAIT_NEUTRAL;s->state_ticks=0u;}
        return;
    }
    if(in->throttle<=c->throttle_idle_max && s->state<G30_RS_VERIFY){
        s->neutral_seen=1u;
        s->state=G30_RS_WAIT_ROLL;
        s->state_ticks=0u;
        return;
    }
    switch((g30_rolling_start_state_t)s->state){
    case G30_RS_WAIT_NEUTRAL:
        return;
    case G30_RS_WAIT_ROLL:
        if(!s->neutral_seen)return;
        if(in->throttle<=c->throttle_idle_max)return;
        if(!in->rolling_ready || in->rolling_step>=0)return;
        s->seed_phase=in->rolling_phase;
        s->seed_step=in->rolling_step;
        s->seed_requested=1u;
        s->verify_good_ticks=0u;
        s->phase_error_abs_sum=0u;
        s->state_ticks=0u;
        s->state=G30_RS_VERIFY;
        return;
    case G30_RS_VERIFY: {
        s->state_ticks++;
        if(in->throttle<=c->throttle_idle_max || !in->rolling_ready){fail(s,G30_RS_ABORT_ROLL_LOST);return;}
        if(!in->observer_valid){s->verify_good_ticks=0u;s->phase_error_abs_sum=0u;if(s->state_ticks>=c->verify_timeout_ticks)fail(s,G30_RS_ABORT_TIMEOUT);return;}
        if(!in->flux_target_uwb || in->flux_magnitude_uwb<in->flux_target_uwb/2u ||
           in->flux_magnitude_uwb>((in->flux_target_uwb*3u)/2u)){fail(s,G30_RS_ABORT_MAGNITUDE);return;}
        const uint16_t pe=abs16s(phase_delta(in->observer_phase,in->rolling_phase));
        if(pe>=16384u){fail(s,G30_RS_ABORT_PHASE);return;}
        if(pe>=c->max_now_phase_error){s->verify_good_ticks=0u;s->phase_error_abs_sum=0u;if(s->state_ticks>=c->verify_timeout_ticks)fail(s,G30_RS_ABORT_TIMEOUT);return;}
        const uint16_t st=abs16s(in->observer_step);
        if(st>c->max_abs_phase_step || in->observer_step>(int16_t)c->max_positive_phase_step){fail(s,G30_RS_ABORT_PHASE_STEP);return;}
        if(in->observer_clamp_delta){fail(s,G30_RS_ABORT_CLAMP);return;}
        if(s->verify_good_ticks<65535u)s->verify_good_ticks++;
        s->phase_error_abs_sum+=pe;
        if(s->verify_good_ticks>=c->verify_ticks){
            const uint32_t avg=s->phase_error_abs_sum/s->verify_good_ticks;
            if(avg>=c->max_avg_phase_error){fail(s,G30_RS_ABORT_PHASE);return;}
            s->state=G30_RS_RAMP;s->state_ticks=0u;s->iq_ref_counts=0u;s->closed_loop_allowed=1u;
        }else if(s->state_ticks>=c->verify_timeout_ticks)fail(s,G30_RS_ABORT_TIMEOUT);
        return;
    }
    case G30_RS_RAMP:
        if(in->throttle<=c->throttle_idle_max || !in->rolling_ready || !in->observer_valid){fail(s,G30_RS_ABORT_OBSERVER);return;}
        s->state_ticks++;
        if(c->ramp_ticks){
            uint32_t q=((uint32_t)c->target_iq_counts*s->state_ticks)/c->ramp_ticks;
            if(q>c->target_iq_counts)q=c->target_iq_counts;
            s->iq_ref_counts=(uint16_t)q;
        }else s->iq_ref_counts=c->target_iq_counts;
        if(s->state_ticks>=c->ramp_ticks){s->state=G30_RS_CLOSED;s->state_ticks=0u;s->iq_ref_counts=c->target_iq_counts;}
        return;
    case G30_RS_CLOSED:
        if(in->throttle<=c->throttle_idle_max || !in->observer_valid){fail(s,G30_RS_ABORT_OBSERVER);return;}
        s->closed_loop_allowed=1u;s->iq_ref_counts=c->target_iq_counts;return;
    case G30_RS_FAULT:
    default:return;
    }
}
