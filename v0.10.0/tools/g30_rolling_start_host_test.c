#include <stdio.h>
#include "g30_rolling_start.h"
#define T(x) do{if(!(x)){fprintf(stderr,"FAIL %d %s\n",__LINE__,#x);return 1;}}while(0)
int main(void){
    g30_rolling_start_t s;g30_rolling_start_config_t c;g30_rolling_start_input_t in={0};
    g30_rolling_start_default_config(&c);g30_rolling_start_init(&s);
    in.dash_valid=1;in.motor_params_valid=1;in.throttle=0;g30_rolling_start_step(&s,&c,&in);
    T(s.neutral_seen&&s.state==G30_RS_WAIT_ROLL);
    in.throttle=50;in.rolling_ready=1;in.rolling_phase=20000;in.rolling_step=-500;g30_rolling_start_step(&s,&c,&in);
    T(s.seed_requested&&s.state==G30_RS_VERIFY&&s.seed_step==-500);
    in.observer_valid=1;in.observer_phase=20100;in.observer_step=-500;in.flux_target_uwb=15000;in.flux_magnitude_uwb=15000;
    for(unsigned i=0;i<c.verify_ticks;i++)g30_rolling_start_step(&s,&c,&in);
    T(s.state==G30_RS_RAMP&&s.closed_loop_allowed);
    for(unsigned i=0;i<c.ramp_ticks;i++)g30_rolling_start_step(&s,&c,&in);
    T(s.state==G30_RS_CLOSED&&s.iq_ref_counts==c.target_iq_counts);
    in.brake=1;g30_rolling_start_step(&s,&c,&in);T(s.state==G30_RS_FAULT&&s.iq_ref_counts==0);
    g30_rolling_start_init(&s);in=(g30_rolling_start_input_t){0};in.dash_valid=1;in.motor_params_valid=1;in.throttle=0;g30_rolling_start_step(&s,&c,&in);
    in.throttle=50;in.rolling_ready=1;in.rolling_phase=1000;in.rolling_step=500;g30_rolling_start_step(&s,&c,&in);T(s.state==G30_RS_WAIT_ROLL&&!s.seed_requested);
    in.rolling_step=-500;g30_rolling_start_step(&s,&c,&in);T(s.state==G30_RS_VERIFY);
    in.observer_valid=1;in.observer_phase=1000;in.observer_step=-2000;in.flux_target_uwb=15000;in.flux_magnitude_uwb=15000;g30_rolling_start_step(&s,&c,&in);
    T(s.state==G30_RS_FAULT&&s.abort_reason==G30_RS_ABORT_PHASE_STEP);
    puts("PASS G30 rolling start: neutral->kick, FO69 observer confidence, zero-to-Iq ramp, fail-closed");
    return 0;
}
