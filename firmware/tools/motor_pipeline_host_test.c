#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "motor_pipeline.h"
#include "stock_current_frontend.h"

typedef struct {
    unsigned gate_on, gate_off, pwm_writes, adc_routes;
    uint16_t c1,c2,c3;
    uint32_t adc1_jsqr,adc2_jsqr;
    unsigned reject_pwm,reject_adc,reject_arm;
    unsigned event_seq;
} fake_hw_t;
static void ensure(unsigned v,const char *what){if(!v){fprintf(stderr,"FAILED %s\n",what);exit(1);}}
static uint8_t arm(void *ctx){fake_hw_t *h=ctx;if(h->reject_arm)return 0;h->gate_on=1;return 1;}
static void off(void *ctx){fake_hw_t *h=ctx;h->gate_on=0;h->gate_off++;}
static void coast(void *ctx){fake_hw_t *h=ctx;h->gate_on=0;h->gate_off++;}
static uint8_t route(void *ctx,uint32_t a,uint32_t b){fake_hw_t *h=ctx;if(h->reject_adc)return 0;h->adc1_jsqr=a;h->adc2_jsqr=b;h->adc_routes++;return 1;}
static uint8_t pwm(void *ctx,uint16_t a,uint16_t b,uint16_t c){fake_hw_t *h=ctx;if(h->reject_pwm)return 0;ensure(a<=1999&&b<=1999&&c<=1999,"PWM duty bounds");h->c1=a;h->c2=b;h->c3=c;h->pwm_writes++;return 1;}
static foc_adc_sample_t sample(motor_pipeline_t *p, fake_hw_t *h){
    (void)h;
    const stock_adc_pair_t pair=stock_current_pair_for_sector(p->adc.expected_sector);
    ensure(p->adc.expected_jsqr1==stock_current_jsqr_one(pair.adc1_channel),"ADC1 sector route");
    ensure(p->adc.expected_jsqr2==stock_current_jsqr_one(pair.adc2_channel),"ADC2 sector route");
    foc_adc_sample_t s={0};s.sequence=++h->event_seq;s.sector=p->adc.expected_sector;
    s.adc1_jsqr=h->adc1_jsqr;s.adc2_jsqr=h->adc2_jsqr;
    s.adc1_sr=4u;s.adc2_sr=4u;s.adc1_jdr1=2048;s.adc2_jdr1=2048;
    s.dual_injected_simultaneous=1;s.sample_window_valid=1;return s;
}
static void ready(motor_pipeline_t *p,sensorless_control_t *c,fake_hw_t *h){
    sensorless_control_init(c);
    ensure(sensorless_control_set_motor_params(c,200000,300000,5000),"motor params");
    motor_pipeline_io_t io={arm,off,route,pwm};
    motor_pipeline_init(p,c,io,h,1999u,700u);
}
int main(void){
    const uint16_t offsets[3]={2048,2048,2048};
    for(unsigned kind=0;kind<10;kind++){
        fake_hw_t hw={0};motor_pipeline_t p;sensorless_control_t c;ready(&p,&c,&hw);
        ensure(!motor_pipeline_start(&p,offsets,0,1,80,0,1,0),"qualification denies gate");
        ensure(!hw.gate_on,"unqualified never arms");
        ensure(motor_pipeline_start(&p,offsets,1,1,80,0,1,1),"qualified mock start");
        ensure(hw.gate_on && p.powered,"mock arm happened");
        for(unsigned n=0;n<16;n++){
            foc_adc_sample_t s=sample(&p,&hw);
            ensure(motor_pipeline_on_adc(&p,&s,2700u),"full ADC reconstruction to FOC SVPWM to PWM");
        }
        ensure(hw.pwm_writes>=5,"FOC output wrote PWM");
        ensure(hw.adc_routes>=17,"ADC channels rescheduled each sector");
        if(kind==0){motor_pipeline_input(&p,0,0,1,5);ensure(p.last_stop_reason==MOTOR_STOP_THROTTLE,"throttle coast");}
        if(kind==1){motor_pipeline_input(&p,80,1,1,5);ensure(p.last_stop_reason==MOTOR_STOP_BRAKE,"brake coast");}
        if(kind==2){motor_pipeline_input(&p,80,0,0,5);ensure(p.last_stop_reason==MOTOR_STOP_DASH,"dashboard invalid coast");}
        if(kind==3){motor_pipeline_watchdog(&p,260,250);ensure(p.last_stop_reason==MOTOR_STOP_DASH,"watchdog coast");}
        if(kind==4){foc_adc_sample_t s=sample(&p,&hw);s.adc2_sr=0;ensure(!motor_pipeline_on_adc(&p,&s,2700),"ADC JEOC fault");ensure(p.last_stop_reason==MOTOR_STOP_ADC,"ADC coast");}
        if(kind==5){foc_adc_sample_t s=sample(&p,&hw);s.sample_window_valid=0;ensure(!motor_pipeline_on_adc(&p,&s,2700),"ADC window fault");ensure(p.last_stop_reason==MOTOR_STOP_ADC,"window coast");}
        if(kind==6){hw.reject_pwm=1;for(unsigned n=0;n<4;n++){foc_adc_sample_t s=sample(&p,&hw);(void)motor_pipeline_on_adc(&p,&s,2700);}ensure(p.last_stop_reason==MOTOR_STOP_PWM,"PWM write fault coast");}
        if(kind==7){hw.reject_adc=1;foc_adc_sample_t s=sample(&p,&hw);ensure(!motor_pipeline_on_adc(&p,&s,2700),"ADC routing write refused");ensure(p.last_stop_reason==MOTOR_STOP_ADC,"ADC routing coast");}
        if(kind==8){c.fault=3;foc_adc_sample_t s=sample(&p,&hw);ensure(!motor_pipeline_on_adc(&p,&s,2700),"observer fault refused");ensure(p.last_stop_reason==MOTOR_STOP_ADC,"observer coast");}
        if(kind==9){
            /* 2048+750 is below ADC rail but reconstructed current over
             * 700 must be rejected *before* PWM for that FOC tick. */
            unsigned oldpwm=hw.pwm_writes;
            foc_adc_sample_t s=sample(&p,&hw);s.adc1_jdr1=2798;
            ensure(!motor_pipeline_on_adc(&p,&s,2700),"overcurrent rejected");
            ensure(hw.pwm_writes==oldpwm,"no PWM following current trip");
            ensure(p.last_stop_reason==MOTOR_STOP_FAULT,"overcurrent coast");
        }
        ensure(!p.powered&&!hw.gate_on&&hw.gate_off>0,"gate physically off before returning");
        ensure(c.state==SENSORLESS_STOP&&!c.drive_request,"FOC stopped");
        if(kind>=4){
            ensure(!motor_pipeline_start(&p,offsets,1,1,80,0,1,1),"fault remains latched");
            ensure(!hw.gate_on,"latched fault never re-arms");
        }
    }
    /* Regression: normal brake is coast, never a terminal fault. After
     * releasing brake and returning throttle to neutral, rolling start is
     * permitted again. A held throttle must not cause surprise re-arm. */
    {
        fake_hw_t hw={0};motor_pipeline_t p;sensorless_control_t c;
        ready(&p,&c,&hw);
        p.io.coast=coast;
        p.neutral_seen=1u;
        ensure(motor_pipeline_start_rolling(&p,offsets,1,1,80,0,1,10,
                                            1000u,-120),
               "brake regression initial rolling start");
        motor_pipeline_input(&p,80,1,1,11);
        ensure(!p.powered && !hw.gate_on &&
               p.last_stop_reason==MOTOR_STOP_BRAKE && !p.fault_latched,
               "brake isolates bridge without latching fault");
        ensure(!p.neutral_seen,
               "held throttle does not auto rearm after brake");
        motor_pipeline_input(&p,80,0,1,12);
        ensure(!p.neutral_seen,"brake release with held throttle stays disarmed");
        motor_pipeline_input(&p,0,0,1,13);
        ensure(p.neutral_seen,"neutral after brake requalifies start");
        ensure(motor_pipeline_start_rolling(&p,offsets,1,1,80,0,1,14,
                                            1000u,-120),
               "rolling rearm after brake release and fresh throttle");
        ensure(p.powered && hw.gate_on && !p.neutral_seen,
               "second motor start actually rearmed");
        motor_pipeline_input(&p,0,0,1,15);
        ensure(!p.powered && !p.fault_latched,
               "second normal coast remains reversible");
    }
    /* Repeat start-permission edge cases without invoking motor power. */
    {
        fake_hw_t hw={0};motor_pipeline_t p;sensorless_control_t c;
        ready(&p,&c,&hw);
        motor_pipeline_input(&p,0,0,1,20);
        ensure(p.neutral_seen,"neutral observed when coasting");
        motor_pipeline_input(&p,0,65,1,21);
        ensure(!p.neutral_seen,"brake clears earlier neutral");
        motor_pipeline_input(&p,100,65,1,22);
        motor_pipeline_input(&p,100,0,1,23);
        ensure(!p.neutral_seen,"gas held through brake release is not fresh neutral");
        motor_pipeline_input(&p,0,0,1,24);
        ensure(p.neutral_seen,"fresh neutral after brake clears rearm condition");
        motor_pipeline_input(&p,100,0,0,25);
        ensure(!p.neutral_seen,"invalid control clears neutral eligibility");
        ensure(!hw.gate_on && !p.powered,"no gate action while testing input state");
    }
    puts("PASS: ADC->sector reconstruction->sensorless FOC/SVPWM->TIM1 callback->gate with 10 stop/fault cases");
    return 0;
}
