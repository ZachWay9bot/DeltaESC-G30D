#include <assert.h>
#include <stdint.h>
#include <string.h>
#include "sensorless_control.h"

static void zero_i(phase_current_counts_t *i){ i->ia=0; i->ib=0; i->ic=0; }

int main(void){
    sensorless_control_t s;
    phase_current_counts_t i;
    memset(&s,0,sizeof(s)); zero_i(&i);
    sensorless_control_init(&s);
    assert(s.state==SENSORLESS_STOP && s.drive_request==0);
    assert(s.motor_params_valid==0);
    assert(sensorless_control_set_run_current_ma(&s,500)==1);
    assert(sensorless_control_set_run_current_ma(&s,99)==0);
    assert(sensorless_control_set_motor_params(&s,100000,100000,15000)==1);
    assert(s.motor_params_valid==1);

    sensorless_control_set_drive(&s,1);
    for(unsigned n=0;n<805;n++) sensorless_control_step(&s,i,2600,1999,1);
    assert(s.state==SENSORLESS_OPEN_LOOP);
    assert(s.ccr1<1999 && s.ccr2<1999 && s.ccr3<1999);
    assert(s.current_sum_counts==0);
#if !MOTOR_VOLTAGE_BENCH
    /* Valid physical R/L parameters must replace the fallback PI gains. */
    assert(s.pi_kp_mod_per_count != 6u || s.pi_ki_mod_per_count != 1u);
#endif

#if !SENSORLESS_CLOSED_LOOP_ALLOWED
    for(unsigned n=0;n<12100;n++) sensorless_control_step(&s,i,2600,1999,1);
    assert(s.drive_request==0);
    assert(s.state==SENSORLESS_STOP);
    assert(s.fault==0);
#endif

    sensorless_control_stop(&s);
    sensorless_control_step(&s,i,2600,1999,0);
    assert(s.ccr1==1000 && s.ccr2==1000 && s.ccr3==1000);
    return 0;
}
