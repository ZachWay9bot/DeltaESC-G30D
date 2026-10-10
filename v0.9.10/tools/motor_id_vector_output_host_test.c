/* Point 2.1: vector_mv -> shared SVPWM -> sector ADC -> TIM1 CCR -> arm.
 * Fake register callbacks only; does NOT pretend hardware current measurement. */
#include "motor_id_vector_output.h"
#include "sensorless_control.h"
#include "stock_current_frontend.h"
#include <stdio.h>
#include <stdint.h>
#define T(c) do {if(!(c)){fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#c);return 1;}}while(0)
typedef struct {int n_arm,n_pwm,n_adc,n_off,fail_at,live,raw;int order[16];int n_order;uint16_t cc[3];uint32_t jsqr1,jsqr2;} mock_t;
static void record(mock_t *m,int x){if(m->n_order<16)m->order[m->n_order++]=x;}
static uint8_t adc(void *c,uint32_t a,uint32_t b){mock_t *m=c; m->n_adc++;record(m,1);m->jsqr1=a;m->jsqr2=b;return (uint8_t)(m->fail_at!=1);}
static uint8_t arm(void *c){mock_t *m=c;m->n_arm++;record(m,2);if(m->fail_at==2)return 0;m->live=1;return 1;}
static uint8_t pwm(void *c,uint16_t a,uint16_t b,uint16_t d){mock_t *m=c;m->n_pwm++;record(m,3);if(m->fail_at==3)return 0;if(a>4000||b>4000||d>4000)return 0;m->cc[0]=a;m->cc[1]=b;m->cc[2]=d;return 1;}
static void off(void *c){mock_t *m=c;m->n_off++;record(m,4);m->live=0;}
static motor_id_vector_io_t io={adc,pwm,arm,off};
static int trajectory(void){
 mock_t m={0};motor_id_vector_output_t v;motor_id_vector_output_init(&v,io,&m);
 T(motor_id_vector_output_request(&v,500,0,36000,4000,1,0,0));
 T(m.live&&m.n_adc==1&&m.n_arm==1&&m.n_pwm==1);
 T(m.order[0]==1 && m.order[1]==2 && m.order[2]==3);
 T(v.sector==stock_current_sector_from_ab(500,0)); /* phase 0 is positive alpha */
 sensorless_id_pwm_t out;T(sensorless_id_voltage_to_pwm(500,0,36000,4000,&out));
 T(out.ccr1==m.cc[0]&&out.ccr2==m.cc[1]&&out.ccr3==m.cc[2]);
 T(out.alpha_q15>0 && out.beta_q15==0);
 T(sensorless_id_voltage_to_pwm(500,16384,36000,4000,&out));
 T(out.alpha_q15==0 && out.beta_q15>0);
 T(sensorless_id_voltage_to_pwm(500,32768,36000,4000,&out));
 T(out.alpha_q15<0 && out.beta_q15==0);
 T(sensorless_id_voltage_to_pwm(500,49152,36000,4000,&out));
 T(out.alpha_q15==0 && out.beta_q15<0);
 T(m.jsqr1==stock_current_jsqr_one(stock_current_pair_for_sector(v.sector).adc1_channel));
 T(m.jsqr2==stock_current_jsqr_one(stock_current_pair_for_sector(v.sector).adc2_channel));
 /* Every commanded rotor angle is converted with same standard FOC SVM */
 for(unsigned phase=0;phase<65536;phase+=257){
   T(motor_id_vector_output_request(&v,750,(uint16_t)phase,36000,4000,1,0,0));
   T(v.sector>=1&&v.sector<=6);
   T(m.cc[0]<=4000 && m.cc[1]<=4000 && m.cc[2]<=4000);
   T(m.jsqr1==stock_current_jsqr_one(stock_current_pair_for_sector(v.sector).adc1_channel));
   T(m.jsqr2==stock_current_jsqr_one(stock_current_pair_for_sector(v.sector).adc2_channel));
 }
 T(m.n_arm==1); /* no unwanted rearm on subsequent vector update */
 motor_id_vector_output_stop(&v);
 T(!m.live && !v.bridge_armed && !v.owning && v.sector==0);
 T(motor_id_vector_output_request(&v,200,16384,36000,4000,1,0,0));
 T(m.n_arm==2);motor_id_vector_output_stop(&v);
 return 0;
}
static int fault_paths(void){
 for(int fail=1;fail<=3;fail++){
  mock_t m={0};m.fail_at=fail;motor_id_vector_output_t v;
  motor_id_vector_output_init(&v,io,&m);
  T(!motor_id_vector_output_request(&v,500,0,36000,4000,1,0,0));
  T(m.n_off==1&&!m.live&&!v.bridge_armed&&!v.owning);
 }
 mock_t m={0};motor_id_vector_output_t v;motor_id_vector_output_init(&v,io,&m);
 T(!motor_id_vector_output_request(&v,500,0,36000,4000,0,0,0));
 T(!motor_id_vector_output_request(&v,500,0,36000,4000,1,1,0));
 T(!motor_id_vector_output_request(&v,500,0,36000,4000,1,0,1));
 T(!motor_id_vector_output_request(&v,500,0,4000,4000,1,0,0));
 T(!motor_id_vector_output_request(&v,0,0,36000,4000,1,0,0));
 T(!motor_id_vector_output_request(&v,2001,0,36000,4000,1,0,0));
 T(!motor_id_vector_output_request(&v,1500,0,5000,4000,1,0,0)); /* no clipping */
 T(m.n_off==7 && m.n_arm==0 && m.n_pwm==0 && m.n_adc==0);
 /* A rejected command after successful vector MUST turn gates off. */
 T(motor_id_vector_output_request(&v,200,0,36000,4000,1,0,0));
 T(m.live);
 T(!motor_id_vector_output_request(&v,200,0,36000,4000,1,0,1));
 T(!m.live&&!v.owning&&!v.bridge_armed);
 return 0;
}
int main(void){if(trajectory()||fault_paths())return 1;
 puts("PASS: ID vector hardware callbacks: ADC sector BEFORE arm BEFORE CCR, full 256-angle rotation, gate-off on fault, fresh rearm");return 0;}