#include "motor_id.h"
#include <stdio.h>
#include <string.h>
#define T(x) do {if(!(x)){printf("FAIL line %d: %s\n",__LINE__,#x);return 1;}}while(0)
static const motor_id_qualification_t qualified={1,1,1,1,1,1};
static motor_id_sample_t make(uint32_t seq,uint8_t stage){
    motor_id_sample_t s={0};s.sequence=seq;s.stage=stage;s.elapsed_us=1000;s.samples_synchronized=1;
    if(stage==MOTOR_ID_RESISTANCE){s.voltage_mv=500;s.current_ma=1000;}
    if(stage==MOTOR_ID_INDUCTANCE){s.voltage_mv=1000;s.current_ma=1000;s.previous_current_ma=900;}
    if(stage==MOTOR_ID_FLUX){s.bemf_mv=200;s.electrical_speed_mrad_s=10000;}
    return s;
}
static int nominal(void){
    motor_id_t m;sensorless_control_t c={0};sensorless_control_init(&c);
    T(motor_id_begin(&m,&qualified));
    for(uint32_t i=0;i<24u;i++){
        uint8_t stage=i<8?MOTOR_ID_RESISTANCE:(i<16?MOTOR_ID_INDUCTANCE:MOTOR_ID_FLUX);
        motor_id_sample_t s=make(i+1u,stage);T(motor_id_accept(&m,&s));
    }
    T(m.result.valid && m.state==MOTOR_ID_READY && !m.fault);
    T(m.result.r_uohm==500000u && m.result.l_nh==5000000u && m.result.flux_uwb==20000u);
    T(m.result.samples_r==8 && m.result.samples_l==8 && m.result.samples_flux==8);
    T(motor_id_commit(&m,&c) && c.motor_params_valid);
    sensorless_control_set_drive(&c,1);
    T(!motor_id_commit(&m,&c));
    T(!motor_id_accept(&m,&(motor_id_sample_t){0}));
    motor_id_abort(&m);T(!m.result.valid && m.state==MOTOR_ID_FAULT && !motor_id_commit(&m,&c));
    return 0;
}
static int faults(void){
    motor_id_t m;motor_id_sample_t s;
    motor_id_qualification_t q=qualified;q.gate_shutdown_verified=0;
    T(!motor_id_begin(&m,&q) && m.fault==MOTOR_ID_BAD_QUALIFICATION);
    T(motor_id_begin(&m,&qualified));s=make(1,MOTOR_ID_RESISTANCE);
    s.brake_active=1;T(!motor_id_accept(&m,&s) && m.state==MOTOR_ID_FAULT);
    T(motor_id_begin(&m,&qualified));s=make(1,MOTOR_ID_RESISTANCE);
    T(motor_id_accept(&m,&s));s.sequence=3;
    T(!motor_id_accept(&m,&s) && m.fault==MOTOR_ID_SEQUENCE);
    T(motor_id_begin(&m,&qualified));s=make(1,MOTOR_ID_RESISTANCE);s.current_ma=0;
    T(!motor_id_accept(&m,&s) && m.fault==MOTOR_ID_RANGE);
    T(motor_id_begin(&m,&qualified));s=make(1,MOTOR_ID_RESISTANCE);s.electrical_speed_mrad_s=1000;
    T(!motor_id_accept(&m,&s) && m.fault==MOTOR_ID_RANGE);
    T(motor_id_begin(&m,&qualified));s=make(1,MOTOR_ID_RESISTANCE);s.elapsed_us=0;
    T(!motor_id_accept(&m,&s) && m.fault==MOTOR_ID_BAD_SAMPLE);
    T(motor_id_begin(&m,&qualified));
    for(unsigned i=0;i<7;i++){s=make(i+1,MOTOR_ID_RESISTANCE);T(motor_id_accept(&m,&s));}
    s=make(8,MOTOR_ID_RESISTANCE);s.voltage_mv=1000;
    T(!motor_id_accept(&m,&s) && m.fault==MOTOR_ID_INCONSISTENT);
    T(motor_id_begin(&m,&qualified));s=make(0xffffffffu,MOTOR_ID_RESISTANCE);
    T(motor_id_accept(&m,&s));s.sequence=0u;T(motor_id_accept(&m,&s));
    T(motor_id_begin(&m,&qualified));s=make(1,MOTOR_ID_INDUCTANCE);
    T(!motor_id_accept(&m,&s) && m.fault==MOTOR_ID_BAD_SAMPLE);
    T(motor_id_begin(&m,&qualified));s=make(1,MOTOR_ID_RESISTANCE);s.stop_requested=1;
    T(!motor_id_accept(&m,&s) && m.fault==MOTOR_ID_BAD_SAMPLE);
    return 0;
}
int main(void){if(nominal()||faults())return 1;puts("PASS motor ID: R/L/flux synthetic units, 3-stage flow, guarded commit, abort, faults");return 0;}
