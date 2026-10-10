#include "motor_id_vector_output.h"
#include "sensorless_control.h"
#include "stock_current_frontend.h"
#include "g30_adc_current_window.h"

static uint8_t reject(motor_id_vector_output_t *v) {
    if (v) {
        /* Never rely on a neutral / 50% duty for motor coast. */
        if(v->io.gate_off)v->io.gate_off(v->ctx);
        v->bridge_armed=0u;v->owning=0u;v->sector=0u;
        if(v->rejected!=0xffffffffu)v->rejected++;
    }
    return 0u;
}
void motor_id_vector_output_init(motor_id_vector_output_t *v,
                                 motor_id_vector_io_t io,void *ctx) {
    if(!v)return;
    *v=(motor_id_vector_output_t){0};v->io=io;v->ctx=ctx;
}
void motor_id_vector_output_stop(motor_id_vector_output_t *v) {
    if(!v)return;
    if(v->io.gate_off)v->io.gate_off(v->ctx);
    v->owning=0u;v->bridge_armed=0u;v->sector=0u;
}
uint8_t motor_id_vector_output_request(motor_id_vector_output_t *v,
                                       int32_t amplitude_mv,uint16_t angle_u16,
                                       uint32_t measured_vbus_mv,uint16_t arr,
                                       uint8_t qualified,uint8_t drive_owns_pwm,
                                       uint8_t brake_or_fault) {
    if(!v)return 0u;
    if(!qualified || drive_owns_pwm || brake_or_fault ||
       !v->io.adc_pair || !v->io.pwm_ccr ||
       !v->io.arm || !v->io.gate_off) return reject(v);
    sensorless_id_pwm_t pwm;
    if (!sensorless_id_voltage_to_pwm(amplitude_mv,angle_u16,
                                     measured_vbus_mv,arr,&pwm))return reject(v);
    /* Equivalent safety aperture to the existing G30 low-side shunt
     * validation. Reject instead of silently clipping Motor ID voltage. */
    const uint8_t sector=stock_current_sector_from_ab(pwm.alpha_q15,pwm.beta_q15);
    if(sector<1u || sector>6u)return reject(v);
    const stock_adc_pair_t pair=stock_current_pair_for_sector(sector);
    const uint16_t ccr[3]={pwm.ccr1,pwm.ccr2,pwm.ccr3};
    const uint16_t sample_max=(uint16_t)(3996u-(26u*6u+64u+32u));
    if(ccr[pair.adc1_channel-3u]>sample_max ||
       ccr[pair.adc2_channel-3u]>sample_max ||
       pwm.ccr1>arr || pwm.ccr2>arr || pwm.ccr3>arr)
        return reject(v);
    /* ADC rank update MUST precede bridge arming. CCR is written through
     * the same TIM1 preload callback as the regular motor pipeline. */
    if(!v->io.adc_pair(v->ctx,stock_current_jsqr_one(pair.adc1_channel),
                              stock_current_jsqr_one(pair.adc2_channel)))return reject(v);
    /* The hardware arm path first loads the neutral CCRs. Therefore
     * on first use it must arm BEFORE loading the requested CCR vector. */
    if(!v->bridge_armed) {
        if(!v->io.arm(v->ctx))return reject(v);
        v->bridge_armed=1u;
    }
    if(!v->io.pwm_ccr(v->ctx,pwm.ccr1,pwm.ccr2,pwm.ccr3))return reject(v);
    v->owning=1u;v->sector=sector;
    if(v->accepted!=0xffffffffu)v->accepted++;
    return 1u;
}