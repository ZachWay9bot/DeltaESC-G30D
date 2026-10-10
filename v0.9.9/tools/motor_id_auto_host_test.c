#include "motor_id_auto.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#define T(x) do { if(!(x)){printf("FAIL line %d: %s\n",__LINE__,#x);return 1;} } while(0)
typedef struct {unsigned off,vector,fail_vector;int32_t voltage;uint16_t angle;uint8_t energized;uint8_t applied;} hw_t;
static void off(void *ctx){hw_t *h=(hw_t*)ctx;h->off++;h->voltage=0;h->energized=0;}
static uint8_t vector(void *ctx,int32_t mv,uint16_t angle){hw_t *h=(hw_t*)ctx;h->vector++;if(h->fail_vector)return 0;h->energized=1;h->voltage=mv;h->angle=angle;return 1;}
static void applied(void *ctx,const motor_id_result_t *r){hw_t *h=(hw_t*)ctx;if(r->valid)h->applied++;}
static motor_id_auto_feedback_t feedback(const motor_id_auto_t *a,hw_t *h){
    motor_id_auto_feedback_t f={0};
    f.adc_valid=1;f.voltage_valid=1;f.throttle_idle=1;f.command_epoch=a->command_epoch;
    if(a->state==ID_AUTO_R_SETTLE || a->state==ID_AUTO_R_COLLECT){
        f.applied_voltage_mv=h->voltage;f.phase_current_ma=2*h->voltage;
    }else if(a->state==ID_AUTO_L_REST){
        f.applied_voltage_mv=0;f.phase_current_ma=0;
    }else if(a->state==ID_AUTO_L_PULSE){
        f.applied_voltage_mv=1000;f.phase_current_ma=1000;
    }else if(a->state==ID_AUTO_FLUX_COAST){
        f.applied_voltage_mv=0;f.phase_current_ma=0;
        f.measured_electrical_speed_mrad_s=10000;
        f.measured_bemf_mv=200;
        f.bemf_valid=1;f.speed_valid=1;
    }else if(a->state==ID_AUTO_FLUX_SPIN){
        f.phase_current_ma=0;f.applied_voltage_mv=h->voltage;
    }
    return f;
}
static int full_test(void){
    sensorless_control_t c; sensorless_control_init(&c);
    hw_t h={0};motor_id_auto_t a;
    motor_id_auto_io_t io={vector,off,applied};
    motor_id_auto_init(&a,io,&h,&c);
    motor_config_txn_t cfg;motor_config_txn_init(&cfg,500u);
    motor_id_auto_bind_config(&a,&cfg);
    motor_id_qualification_t q={1,1,1,1,1,1};
    T(motor_id_auto_begin(&a,&q,0u));
    T(h.energized && a.state==ID_AUTO_R_SETTLE && !c.motor_params_valid);
    for(uint32_t t=250u;t<1500000u;t+=250u){
        motor_id_auto_feedback_t f=feedback(&a,&h);
        T(motor_id_auto_tick(&a,t,&f));
        if(a.state==ID_AUTO_COMPLETE)break;
    }
    T(a.state==ID_AUTO_COMPLETE && h.off>=11 && !h.energized && h.applied==1);
    T(a.id.state==MOTOR_ID_READY && a.id.result.valid && !a.id.fault);
    T(a.id.result.r_uohm==500000 && a.id.result.l_nh==500000 && a.id.result.flux_uwb==20000);
    T(a.id.result.samples_r==8 && a.id.result.samples_l==8 && a.id.result.samples_flux==8);
    T(c.motor_params_valid && c.motor_r_uohm==500000 && c.motor_l_nh==500000 && c.motor_flux_uwb==20000);
    T(cfg.active_valid && cfg.active.r_uohm==500000 && cfg.active.l_nh==500000 && cfg.active.flux_uwb==20000);
    T(cfg.active.test_current_ma==500 && !cfg.pending_mask);
    T(!motor_id_auto_begin(&a,&q,1500000u));
    T(motor_id_auto_tick(&a,1600000u,0));
    return 0;
}
static int refusals(void){
    motor_id_qualification_t q={1,1,1,1,1,1};
    sensorless_control_t c;sensorless_control_init(&c);
    hw_t h={0};motor_id_auto_t a;motor_id_auto_io_t io={vector,off,applied};
    motor_id_auto_init(&a,io,&h,&c);
    q.current_scale_verified=0;
    T(!motor_id_auto_begin(&a,&q,0) && !h.energized && h.vector==0 && !c.motor_params_valid);
    motor_id_auto_init(&a,io,&h,&c);q.current_scale_verified=1;
    c.drive_request=1;
    T(!motor_id_auto_begin(&a,&q,0) && h.vector==0);
    c.drive_request=0;
    h.fail_vector=1;
    T(!motor_id_auto_begin(&a,&q,0) && a.state==ID_AUTO_FAULT && !h.energized);
    motor_id_auto_init(&a,io,&h,&c);h.fail_vector=0;
    T(motor_id_auto_begin(&a,&q,0));
    motor_id_auto_feedback_t f=feedback(&a,&h);f.brake_active=1;
    T(!motor_id_auto_tick(&a,250,&f) && a.state==ID_AUTO_FAULT && !h.energized && !c.motor_params_valid);
    motor_id_auto_init(&a,io,&h,&c);
    T(motor_id_auto_begin(&a,&q,0));f=feedback(&a,&h);
    T(!motor_id_auto_tick(&a,60000,&f) && a.fault==ID_AUTO_FAULT_WATCHDOG && !h.energized);
    motor_id_auto_init(&a,io,&h,&c);
    T(motor_id_auto_begin(&a,&q,0));f=feedback(&a,&h);
    for(uint32_t t=250;t<=8000;t+=250){f=feedback(&a,&h);T(motor_id_auto_tick(&a,t,&f));}
    f=feedback(&a,&h);
    f.adc_valid=0;
    T(!motor_id_auto_tick(&a,8250,&f) && a.fault==ID_AUTO_FAULT_SENSOR && !h.energized);
    motor_id_auto_init(&a,io,&h,&c);
    T(motor_id_auto_begin(&a,&q,0));
    motor_id_auto_abort(&a);T(a.state==ID_AUTO_FAULT && !h.energized && !a.id.result.valid);
    return 0;
}
static int no_synthetic_flux(void) {
    sensorless_control_t c;sensorless_control_init(&c);
    hw_t h={0};motor_id_auto_t a;motor_id_auto_io_t io={vector,off,applied};
    motor_id_qualification_t q={1,1,1,1,1,1};
    motor_id_auto_init(&a,io,&h,&c);T(motor_id_auto_begin(&a,&q,0));
    for(uint32_t t=250u;t<1300000u;t+=250u){
        motor_id_auto_feedback_t f=feedback(&a,&h);
        if(a.state==ID_AUTO_FLUX_COAST) f.speed_valid=0; /* commanded spin is NOT evidence */
        uint8_t ok=motor_id_auto_tick(&a,t,&f);
        if(!ok){T(a.state==ID_AUTO_FAULT && a.fault==ID_AUTO_FAULT_SENSOR && !h.energized && !c.motor_params_valid);return 0;}
    }
    T(0);
    return 1;
}
int main(void){if(full_test()||refusals()||no_synthetic_flux())return 1;
puts("PASS motor ID auto: R plateau, L pulses, coast flux, guarded adoption, brake/timeout/ADC rejection, no fake speed");return 0;}