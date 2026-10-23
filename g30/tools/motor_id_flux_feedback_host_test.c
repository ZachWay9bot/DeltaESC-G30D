#include "motor_id_flux_feedback.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static void need(int ok,const char *text){if(!ok){fprintf(stderr,"FAIL %s\n",text);exit(1);}}
static motor_id_t identify(void){motor_id_t m={0};m.state=MOTOR_ID_FLUX;m.result.r_uohm=95000;m.result.l_nh=70000;return m;}
static motor_id_auto_feedback_t sample(int32_t va,int32_t vb,int32_t ia,int32_t ib){
 motor_id_auto_feedback_t f={0};f.adc_valid=f.voltage_valid=1;f.voltage_alpha_mv=va;f.voltage_beta_mv=vb;
 f.current_alpha_ma=ia;f.current_beta_ma=ib;return f;
}
int main(void){
 motor_id_flux_feedback_t q={0};motor_id_t id=identify();int n=0;long speed=0,emf=0;
 /* Rotating BEMF: voltage samples, not requested field commands. */
 for(uint32_t t=250;t<=720000;t+=250){
   const double theta=10.0*(double)t/1000000.0;
   motor_id_auto_feedback_t f=sample((int32_t)lrint(170.0*cos(theta)),(int32_t)lrint(170.0*sin(theta)),0,0);
   motor_id_flux_feedback_observe(&q,&id,t,&f);
   if(f.bemf_valid){need(f.speed_valid,"speed pair");n++;speed+=f.measured_electrical_speed_mrad_s;emf+=f.measured_bemf_mv;}
 }
 need(n>=8,"independent rotating BEMF qualified");
 const long mean_speed=speed/n,mean_emf=emf/n;
 need(mean_speed>9200&&mean_speed<10800,"observed speed from BEMF phase");
 need(mean_emf>155&&mean_emf<175,"observed BEMF magnitude");
 motor_id_flux_feedback_reset(&q);n=0;
 /* Locked winding with only resistive excitation: no artificial rotor speed. */
 for(uint32_t t=250;t<=600000;t+=250){
    motor_id_auto_feedback_t f=sample(95,0,1000,0);
    motor_id_flux_feedback_observe(&q,&id,t,&f);
    n+=(int)f.bemf_valid;
 }
 need(n==0,"stalled rotor never supplies speed");
 motor_id_flux_feedback_reset(&q);
 motor_id_auto_feedback_t f=sample(200,0,0,0);
 motor_id_flux_feedback_observe(&q,&id,250,&f);
 f=sample(200,0,0,0);motor_id_flux_feedback_observe(&q,&id,5000,&f);
 need(!f.bemf_valid && q.invalid_windows>0,"stale ADC samples invalidate observer");
 printf("PASS Motor-ID measured BEMF/speed, CORDIC, stall and stale-sample veto (%d rotating windows, %ld mrad/s, %ld mV)\n",n,mean_speed,mean_emf);
}
