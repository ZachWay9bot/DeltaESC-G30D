#include "motor_id_adc_feedback.h"
#include "stock_current_frontend.h"
#include "g30_adc_current_offsets.h"

static const int16_t sine[256]={
#include "sin_q15.inc"
};
static int32_t sine_q15(uint16_t angle){return sine[angle>>8];}
static int32_t cosine_q15(uint16_t angle){return sine[(uint8_t)((angle>>8)+64u)];}
void motor_id_adc_feedback_init(motor_id_adc_feedback_t *f){
    if(f)*f=(motor_id_adc_feedback_t){0};
}
void motor_id_adc_feedback_queue(motor_id_adc_feedback_t *f,
    uint16_t c1,uint16_t c2,uint16_t c3,uint16_t angle,
    uint8_t sector,uint32_t epoch){
    if(!f)return;
    f->queued.ccr[0]=c1;f->queued.ccr[1]=c2;f->queued.ccr[2]=c3;
    f->queued.angle_u16=angle;f->queued.sector=sector;f->queued.epoch=epoch;
    f->pending=1u;
}
void motor_id_adc_feedback_on_update(motor_id_adc_feedback_t *f,uint32_t cycle){
    if(!f)return;
    if(f->pending){
        f->active=f->queued;
        f->active_valid=1u;
        f->pending=0u;
    }
    if(f->active_valid)f->active_update_cycle=cycle;
}
void motor_id_adc_feedback_off(motor_id_adc_feedback_t *f){
    if(!f)return;
    f->active_valid=0u;f->pending=0u;f->active.epoch=0u;
}
static uint8_t fail(motor_id_adc_feedback_t *f){
    if(f && f->samples_rejected!=0xffffffffu)f->samples_rejected++;
    return 0u;
}
uint8_t motor_id_adc_feedback_capture(motor_id_adc_feedback_t *f,
    const motor_id_adc_input_t *in,motor_id_auto_feedback_t *out){
    if(!f||!in||!out)return 0u;
    *out=(motor_id_auto_feedback_t){0};
    const motor_id_pwm_frame_t *a=&f->active;
    if(!f->active_valid || !in->adc1_jeoc || !in->adc2_jeoc ||
       !in->dual_mode_valid || !in->window_valid ||
       !in->current_scale_qualified || !in->timing_qualified ||
       !in->clock_hz || !in->tim1_arr || a->sector<1u || a->sector>6u ||
       !a->epoch || (uint32_t)(in->now_cycles-f->active_update_cycle)==0u ||
       (uint32_t)(in->now_cycles-f->active_update_cycle)>in->clock_hz/4000u ||
       in->vbus_age_ms>100u || in->vbus_mv<5000u || in->vbus_mv>60000u ||
       in->adc1_raw>4095u || in->adc2_raw>4095u) return fail(f);
    const stock_adc_pair_t pair=stock_current_pair_for_sector(a->sector);
    /* Active CCR window, not the readable CCR preload register. */
    if(a->ccr[0]>in->tim1_arr || a->ccr[1]>in->tim1_arr ||
       a->ccr[2]>in->tim1_arr ||
       a->ccr[pair.adc1_channel-3u]>3744u ||
       a->ccr[pair.adc2_channel-3u]>3744u)return fail(f);
    if(in->jsqr1!=stock_current_jsqr_one(pair.adc1_channel) ||
       in->jsqr2!=stock_current_jsqr_one(pair.adc2_channel))return fail(f);
    uint16_t adc2=in->adc2_raw;
    if(!g30_adc2_current_offset_normalize(a->sector,adc2,in->offset,
       in->adc2_ch4_offset,in->adc2_ch5_offset,
       in->adc2_ch4_offset_valid,in->adc2_ch5_offset_valid,&adc2))return fail(f);
    const stock_phase_currents_t p=stock_current_reconstruct_counts(a->sector,
        in->adc1_raw,adc2,in->offset);
    /* Reject all three raw phase currents BEFORE projection. Otherwise
     * a large quadrature current could hide behind a tiny measured d axis. */
    if(p.ia>40 || p.ia< -40 || p.ib>40 || p.ib< -40 ||
       p.ic>40 || p.ic< -40)return fail(f);
    /* Clarke alpha=Ia, beta=(Ib-Ic)/sqrt(3); then d axis.
     * Convert counts to nominal mA BEFORE Q15 angle multiplication;
     * doing the reverse loses one whole 50mA ADC LSB at angle zero. */
    const int32_t ia_ma=stock_current_nominal_counts_to_ma(p.ia);
    const int32_t ib_ma=stock_current_nominal_counts_to_ma(p.ib);
    const int32_t ic_ma=stock_current_nominal_counts_to_ma(p.ic);
    const int32_t ibeta_ma=(ib_ma-ic_ma)*18919/32768;
    const int32_t id_ma=(ia_ma*cosine_q15(a->angle_u16))/32768 +
                        (ibeta_ma*sine_q15(a->angle_u16))/32768;
    if(id_ma>2000 || id_ma< -2000)return fail(f);
    /* Actual DC bus measurement multiplied by active CCR voltage vector.
     * CCx preload values cannot be used until TIM1 update IRQ publishes them.
     * Scalar d-axis voltage, NOT the raw command amplitude. */
    const int32_t va=(int32_t)(in->vbus_mv *
         (2*(int32_t)a->ccr[0]-(int32_t)a->ccr[1]-(int32_t)a->ccr[2]) /
         (3*(int32_t)in->tim1_arr));
    const int32_t vb=((int32_t)in->vbus_mv *
         ((int32_t)a->ccr[1]-(int32_t)a->ccr[2]) /
         (int32_t)in->tim1_arr)*18919/32768;
    const int32_t vd=(va*cosine_q15(a->angle_u16))/32768 +
                     (vb*sine_q15(a->angle_u16))/32768;
    if(vd>2000 || vd< -2000)return fail(f);
    out->command_epoch=a->epoch;
    out->applied_voltage_mv=vd;
    out->phase_current_ma=id_ma;
    out->adc_valid=1u;out->voltage_valid=1u;
    /* BEMF/speed explicitly remain unavailable until separate point 2.3. */
    if(f->samples_accepted!=0xffffffffu)f->samples_accepted++;
    return 1u;
}
