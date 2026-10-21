#include <stdio.h>
#include <stdint.h>
#include "sensorless_control.h"
#define TEST(x) do {if(!(x)){printf("FAIL %s line %d\n",#x,__LINE__);return 1;}}while(0)
static int neutral(const sensorless_control_t *s,uint16_t c){
    return s->ccr1==c&&s->ccr2==c&&s->ccr3==c;
}
int main(void){
    sensorless_control_t s={0};
    phase_current_counts_t i={0};
    sensorless_control_init(&s);
    TEST(neutral(&s,1000));
    TEST(s.state==SENSORLESS_STOP);
    TEST(sensorless_control_set_motor_params(&s,100000,100000,15000));
    sensorless_control_set_drive(&s,1);
    for(unsigned k=0;k<810;k++)sensorless_control_step(&s,i,2600,1999,1);
    TEST(s.state==SENSORLESS_OPEN_LOOP);
    sensorless_control_stop(&s);
    TEST(neutral(&s,1000));
    TEST(s.drive_request==0&&s.state==SENSORLESS_STOP);
    sensorless_control_set_drive(&s,1);
    s.observer_phase=1234;s.bemf_mv=1234;
    sensorless_control_step(&s,i,2600,1999,0);
    TEST(neutral(&s,1000));
    TEST(s.observer_phase==1234&&s.bemf_mv==1234);
    sensorless_control_stop(&s);
    sensorless_control_step(&s,i,2600,1599,0);
    TEST(neutral(&s,800));
    sensorless_control_stop(&s);
    TEST(neutral(&s,800));
#if SENSORLESS_CLOSED_LOOP_ALLOWED
    sensorless_control_init(&s);
    sensorless_control_set_drive(&s,1);
    int fault_seen=0,timeout_tick=0;
    /* Synthetic rotor and deliberately unqualified observer parameters:
       timeout must disable PWM in precisely the faulting tick. */
    for(int k=0;k<12000;k++){
        sensorless_control_step(&s,i,2600,1999,1);
        if(s.state==SENSORLESS_FAULT){
            TEST(s.fault==1);
            TEST(!s.drive_request);
            TEST(neutral(&s,1000));
            TEST(s.id_int==0&&s.iq_int==0);
            fault_seen=1;timeout_tick=k;break;
        }
    }
    TEST(fault_seen&&timeout_tick>=8000);
    sensorless_control_set_drive(&s,1);
    TEST(s.drive_request==0&&s.state==SENSORLESS_FAULT);
    sensorless_control_stop(&s);
    TEST(neutral(&s,1000));
    sensorless_control_set_drive(&s,1);
    TEST(s.drive_request==0&&s.fault==1);
    printf("PASS timeout fault latched and SAME-TICK output neutral at %d\n",timeout_tick);
#else
    puts("PASS synchronous stop, unpowered observer hold and immediate neutral");
#endif
    return 0;
}
