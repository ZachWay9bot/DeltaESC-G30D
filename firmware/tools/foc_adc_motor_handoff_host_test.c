#include <stdio.h>
#include "foc_adc_motor_handoff.h"
#define T(x) do {if(!(x)) {printf("FAIL line %d: %s\n",__LINE__,#x);return 1;}} while(0)
static void mock_kill(void *ctx) { (*(unsigned*)ctx)++; }
int main(void) {
    unsigned killed=0u;
    sensorless_control_t c={0};
    foc_adc_result_t r={.valid=1,.sector=1,.sequence=1,.counts={0,0,0}};
    sensorless_control_init(&c);
    sensorless_control_set_drive(&c,1);
    T(!foc_adc_motor_handoff_step(&c,&r,2500,1999,1,0,mock_kill,&killed));
    T(killed==1 && c.state==SENSORLESS_STOP && c.ccr1==1000 && c.ccr2==1000);
    sensorless_control_set_drive(&c,1);
    T(!foc_adc_motor_handoff_step(&c,&r,2500,1999,1,1,0,&killed));
    T(killed==1 && c.state==SENSORLESS_STOP);
    sensorless_control_set_drive(&c,1);
    T(foc_adc_motor_handoff_step(&c,&r,2500,1999,1,1,mock_kill,&killed));
    T(killed==1 && c.state==SENSORLESS_ALIGN);
    r.valid=0;
    T(!foc_adc_motor_handoff_step(&c,&r,2500,1999,1,1,mock_kill,&killed));
    T(killed==2 && c.state==SENSORLESS_STOP && c.ccr1==1000);
    sensorless_control_set_drive(&c,1);
    c.fault=2; c.state=SENSORLESS_FAULT;
    r.valid=1;
    T(!foc_adc_motor_handoff_step(&c,&r,2500,1999,1,1,mock_kill,&killed));
    T(killed==3 && c.fault==2 && c.state==SENSORLESS_STOP);
    T(!c.drive_request);
    T(!foc_adc_motor_handoff_step(&c,&r,2500,1999,1,1,mock_kill,&killed));
    T(killed==4);
    puts("PASS FOC handoff: hardware-kill callback on sample/qualification/fault, no callback => no torque");
    return 0;
}
