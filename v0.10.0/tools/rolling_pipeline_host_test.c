#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "motor_pipeline.h"
#include "stock_current_frontend.h"
#define T(x) do{if(!(x)){fprintf(stderr,"FAIL %s:%d %s\n",__FILE__,__LINE__,#x);return 1;}}while(0)
typedef struct{unsigned gate_on,gate_off,pwm,adc;uint32_t js1,js2;}hw_t;
static uint8_t arm(void*ctx){((hw_t*)ctx)->gate_on=1;return 1;}
static void off(void*ctx){hw_t*h=ctx;h->gate_on=0;h->gate_off++;}
static uint8_t route(void*ctx,uint32_t a,uint32_t b){hw_t*h=ctx;h->js1=a;h->js2=b;h->adc++;return 1;}
static uint8_t pwm(void*ctx,uint16_t a,uint16_t b,uint16_t c){hw_t*h=ctx;(void)a;(void)b;(void)c;h->pwm++;return 1;}
int main(void){
    const uint16_t offsets[3]={2048,2048,2048};hw_t h={0};motor_pipeline_t p;sensorless_control_t c;
    sensorless_control_init(&c);T(sensorless_control_set_motor_params(&c,200000u,300000u,15000u));
    motor_pipeline_io_t io={arm,off,route,pwm};motor_pipeline_init(&p,&c,io,&h,4000u,700u);
    T(!motor_pipeline_start_rolling(&p,offsets,1,1,80,0,1,1,20000u,-500));T(!h.gate_on);
    motor_pipeline_input(&p,0,0,1,2);T(p.neutral_seen);
    T(motor_pipeline_start_rolling(&p,offsets,1,1,80,0,1,3,20000u,-500));
    T(h.gate_on&&p.powered);T(c.state==SENSORLESS_ROLL_VERIFY);T(c.rolling_start);T(c.rolling_step==-500);
    motor_pipeline_input(&p,80,1,1,4);T(!p.powered&&!h.gate_on&&h.gate_off);T(c.state==SENSORLESS_STOP);
    puts("PASS rolling pipeline: neutral gate, BEMF seed, no ALIGN/OPEN, hardware-first brake stop");
    return 0;
}
