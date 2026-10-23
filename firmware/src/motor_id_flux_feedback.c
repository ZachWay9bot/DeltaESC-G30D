#include "motor_id_flux_feedback.h"

/* Sampling noise at 4 kHz is substantial on G30 two-shunt ADC; require
 * stable EMF magnitude and a >=20 ms independently observed phase interval. */
#define BEMF_MIN_MAG_MV 20u
#define SPEED_WINDOW_MIN_US 20000u
#define SPEED_WINDOW_MAX_US 50000u
#define MAX_CURRENT_ABS_MA 2000
#define MAX_DIDT_STEP_MA 1000
static uint32_t magnitude_i32(int32_t n) {return n<0?(uint32_t)(-(int64_t)n):(uint32_t)n;}
static uint32_t isqrt32(uint32_t x) {
    uint32_t root=0, bit=1u<<30;
    while(bit>x)bit>>=2;
    while(bit){if(x>=root+bit){x-=root+bit;root=(root>>1)+bit;}else root>>=1;bit>>=2;}
    return root;
}
/* Integer CORDIC removes the many-degree phase-velocity ripple of the
 * earlier piecewise-linear atan approximation. No FPU or libm on STM32F103. */
static uint16_t phase_u16(int32_t y,int32_t x){
    static const uint16_t atan_turn16[15]={8192,4836,2555,1297,651,326,163,81,41,20,10,5,3,1,1};
    if(!(x|y))return 0u;
    int32_t phase=0;
    if(x<0){x=-x;y=-y;phase=32768;}
    x*=1024;y*=1024;
    for(unsigned k=0;k<15u;k++){
        const int32_t oldx=x,oldy=y;
        if(oldy>0){x=oldx+(oldy>>(int)k);y=oldy-(oldx>>(int)k);phase+=atan_turn16[k];}
        else{x=oldx-(oldy>>(int)k);y=oldy+(oldx>>(int)k);phase-=atan_turn16[k];}
    }
    return (uint16_t)phase;
}
void motor_id_flux_feedback_reset(motor_id_flux_feedback_t *s){if(s)*s=(motor_id_flux_feedback_t){0};}
void motor_id_flux_feedback_observe(motor_id_flux_feedback_t *s,
    const motor_id_t *id,uint32_t now_us,motor_id_auto_feedback_t *fb){
    if(!fb)return;
    fb->bemf_valid=0u;fb->speed_valid=0u;
    fb->measured_bemf_mv=0u;fb->measured_electrical_speed_mrad_s=0u;
    if(!s||!id||id->state!=MOTOR_ID_FLUX||!fb->adc_valid||!fb->voltage_valid||
       id->result.r_uohm<1000u||id->result.l_nh<1000u||
       id->result.l_nh>500000u||
       magnitude_i32(fb->current_alpha_ma)>MAX_CURRENT_ABS_MA||
       magnitude_i32(fb->current_beta_ma)>MAX_CURRENT_ABS_MA){
        if(s){motor_id_flux_feedback_reset(s);s->invalid_windows++;}
        return;
    }
    const int32_t ia=fb->current_alpha_ma,ib=fb->current_beta_ma;
    const uint32_t dt=now_us-s->previous_us;
    if(!s->primed){s->previous_ia_ma=ia;s->previous_ib_ma=ib;
        s->previous_us=now_us;s->primed=1u;return;}
    if(!dt||dt>1500u){motor_id_flux_feedback_reset(s);s->invalid_windows++;return;}
    const int32_t da=ia-s->previous_ia_ma,db=ib-s->previous_ib_ma;
    s->previous_ia_ma=ia;s->previous_ib_ma=ib;s->previous_us=now_us;
    if(magnitude_i32(da)>MAX_DIDT_STEP_MA||magnitude_i32(db)>MAX_DIDT_STEP_MA){
        s->filtered=0u;s->phase_ready=0u;s->invalid_windows++;return;
    }
    /* Input voltage is reconstructed from the effective CCR shadow and VBUS.
     * A zero-current stalled RL motor ideally yields zero EMF residual.
     * R is mOhm; L is uH; I is mA; time is us => all terms in mV. */
    const int32_t r_mohm=(int32_t)(id->result.r_uohm/1000u);
    const int32_t l_uh=(int32_t)(id->result.l_nh/1000u);
    const int32_t ea=fb->voltage_alpha_mv-(r_mohm*ia)/1000-(l_uh*da)/(int32_t)dt;
    const int32_t eb=fb->voltage_beta_mv -(r_mohm*ib)/1000-(l_uh*db)/(int32_t)dt;
    if(magnitude_i32(ea)>1500u||magnitude_i32(eb)>1500u){
        s->filtered=0u;s->phase_ready=0u;s->invalid_windows++;return;
    }
    if(!s->filtered){s->ea_filtered_mv=ea;s->eb_filtered_mv=eb;s->filtered=1u;}
    else{
        s->ea_filtered_mv+=(ea-s->ea_filtered_mv)/8;
        s->eb_filtered_mv+=(eb-s->eb_filtered_mv)/8;
    }
    const uint32_t ea_abs=magnitude_i32(s->ea_filtered_mv);
    const uint32_t eb_abs=magnitude_i32(s->eb_filtered_mv);
    /* Guard both squares, prior 1.5 kV caps keep these well below uint32. */
    const uint32_t mag=isqrt32(ea_abs*ea_abs+eb_abs*eb_abs);
    if(mag<BEMF_MIN_MAG_MV){s->phase_ready=0u;return;}
    const uint16_t phase=phase_u16(s->eb_filtered_mv,s->ea_filtered_mv);
    if(!s->phase_ready){s->phase_at_window=phase;s->phase_at_us=now_us;s->phase_ready=1u;return;}
    const uint32_t elapsed=now_us-s->phase_at_us;
    if(elapsed<SPEED_WINDOW_MIN_US)return;
    if(elapsed>SPEED_WINDOW_MAX_US){s->phase_at_us=now_us;s->phase_at_window=phase;s->invalid_windows++;return;}
    const int32_t delta=(int16_t)(phase-s->phase_at_window);
    s->phase_at_window=phase;s->phase_at_us=now_us;
    if(delta<=0||delta>12000){s->invalid_windows++;return;}
    /* Turn16 phase delta * 2*pi * 1000 / elapsed_us, with 1e6 us/s:
     * speed_mrad_s = delta_turn16 * 95874 / elapsed_us. */
    const uint32_t speed=((uint32_t)delta*95874u)/elapsed;
    if(speed<1000u||speed>200000u){s->invalid_windows++;return;}
    if(s->valid_windows!=0xffffffffu)s->valid_windows++;
    fb->measured_bemf_mv=mag;
    fb->measured_electrical_speed_mrad_s=speed;
    fb->bemf_valid=1u;fb->speed_valid=1u;
}
