#include "motor_pipeline.h"
#include "stock_current_frontend.h"

#define THROTTLE_IDLE_MAX 3u

void motor_pipeline_init(motor_pipeline_t *p, sensorless_control_t *foc,
                         motor_pipeline_io_t io, void *ctx, uint16_t arr,
                         uint16_t limit) {
    *p = (motor_pipeline_t){0};
    foc_adc_contract_init(&p->adc);
    p->foc = foc; p->io = io; p->io_ctx = ctx; p->arr = arr;
    p->current_hard_limit_counts=limit;
}

void motor_pipeline_stop(motor_pipeline_t *p, uint8_t reason) {
    if (!p) return;
    /* Hardware off FIRST; 50% CCR is NOT motor OFF. */
    if (p->io.gate_off) p->io.gate_off(p->io_ctx);
    p->powered = 0u;
    p->adc.pending = 0u;
    p->adc_qualified = 0u;
    p->last_stop_reason = reason;
    if (reason==MOTOR_STOP_ADC || reason==MOTOR_STOP_FAULT ||
        reason==MOTOR_STOP_PWM) p->fault_latched=1u;
    if (p->foc) {
        /* Hardware is already OFF above. Preserve the exact observer/control
         * state that caused or preceded the stop before sensorless_control_stop
         * clears the rolling state machine. */
        p->stop_foc_state=p->foc->state;
        p->stop_foc_fault=p->foc->fault;
        p->stop_phase_error=p->foc->phase_error;
        p->stop_observer_step=p->foc->observer_step;
        p->stop_observer_phase=p->foc->observer_phase;
        p->stop_control_phase=p->foc->control_phase;
        p->stop_id=(int16_t)p->foc->id;
        p->stop_iq=(int16_t)p->foc->iq;
        p->stop_clamp_delta=(uint16_t)(p->foc->vesc_flux.clamp_events-
                                       p->foc->rolling_clamp_start);
        p->stop_rolling_good_ticks=p->foc->rolling_good_ticks;
        p->stop_rolling_iq_ticks=p->foc->rolling_iq_ticks;
        sensorless_control_stop(p->foc);
    }
}

uint8_t motor_pipeline_start(motor_pipeline_t *p, const uint16_t offsets[3],
                             uint8_t gate_ok, uint8_t adc_ok,
                             uint8_t throttle, uint8_t brake,
                             uint8_t dash_ok, uint32_t now_ms) {
    if (!p || !p->foc) return 0u;
    if (p->powered || !p->io.gate_off || !p->io.arm ||
        !p->io.set_adc_pair || !p->io.set_pwm || !offsets ||
        !gate_ok || !adc_ok || !dash_ok || brake || p->fault_latched ||
        !p->current_hard_limit_counts ||
        throttle <= THROTTLE_IDLE_MAX ||
        p->foc->fault || p->foc->state == SENSORLESS_FAULT ||
        !p->foc->motor_params_valid || !p->arr) {
        motor_pipeline_stop(p, MOTOR_STOP_QUALIFICATION);
        return 0u;
    }
    /* In particular, no old fault may be cleared merely by restarting. */
    if (!foc_adc_contract_reset(&p->adc, 1u)) {
        motor_pipeline_stop(p, MOTOR_STOP_FAULT); return 0u;
    }
    for (unsigned i=0u;i<3u;i++) p->offsets[i]=offsets[i];
    p->last_sector=1u; p->next_sequence=1u; p->sample_divider=0u;
    p->last_dashboard_ms=now_ms;
    p->adc_qualified=1u;
    if (!foc_adc_contract_prepare(&p->adc,p->last_sector,1u,p->offsets,1u) ||
        !p->io.set_adc_pair(p->io_ctx,p->adc.expected_jsqr1,p->adc.expected_jsqr2) ||
        !p->io.set_pwm(p->io_ctx,(uint16_t)((p->arr+1u)/2u),
                       (uint16_t)((p->arr+1u)/2u),(uint16_t)((p->arr+1u)/2u))) {
        motor_pipeline_stop(p, MOTOR_STOP_ADC); return 0u;
    }
    sensorless_control_set_drive(p->foc,1u);
    if (!p->foc->drive_request || !p->io.arm(p->io_ctx)) {
        motor_pipeline_stop(p, MOTOR_STOP_QUALIFICATION); return 0u;
    }
    p->powered=1u;
    p->last_stop_reason=MOTOR_STOP_NONE;
    return 1u;
}

uint8_t motor_pipeline_start_rolling(motor_pipeline_t *p,const uint16_t offsets[3],
                             uint8_t gate_ok,uint8_t adc_ok,
                             uint8_t throttle,uint8_t brake,uint8_t dash_ok,
                             uint32_t now_ms,uint16_t phase_u16,int16_t electrical_step_u16) {
    if (!p || !p->foc || p->powered || !p->io.gate_off || !p->io.arm ||
        !p->io.set_adc_pair || !p->io.set_pwm || !offsets || !gate_ok || !adc_ok ||
        !dash_ok || brake || !p->neutral_seen || p->fault_latched ||
        throttle<=THROTTLE_IDLE_MAX || electrical_step_u16>=0 ||
        !p->current_hard_limit_counts || !p->foc->motor_params_valid || !p->arr) {
        motor_pipeline_stop(p,MOTOR_STOP_QUALIFICATION);return 0u;
    }
    if(!foc_adc_contract_reset(&p->adc,1u)){
        motor_pipeline_stop(p,MOTOR_STOP_FAULT);return 0u;
    }
    for(unsigned i=0u;i<3u;i++)p->offsets[i]=offsets[i];
    p->last_sector=1u;p->next_sequence=1u;p->sample_divider=0u;
    p->last_dashboard_ms=now_ms;p->adc_qualified=1u;
    if(!foc_adc_contract_prepare(&p->adc,p->last_sector,1u,p->offsets,1u) ||
       !p->io.set_adc_pair(p->io_ctx,p->adc.expected_jsqr1,p->adc.expected_jsqr2) ||
       !p->io.set_pwm(p->io_ctx,(uint16_t)((p->arr+1u)/2u),
                      (uint16_t)((p->arr+1u)/2u),(uint16_t)((p->arr+1u)/2u)) ||
       !sensorless_control_start_rolling(p->foc,phase_u16,electrical_step_u16) ||
       !p->io.arm(p->io_ctx)) {
        motor_pipeline_stop(p,MOTOR_STOP_QUALIFICATION);return 0u;
    }
    p->powered=1u;p->neutral_seen=0u;p->last_stop_reason=MOTOR_STOP_NONE;
    return 1u;
}

void motor_pipeline_input(motor_pipeline_t *p, uint8_t throttle,
                          uint8_t brake, uint8_t dashboard_valid,
                          uint32_t now_ms) {
    if (!p) return;
    if(!p->powered && dashboard_valid && !brake && throttle<=THROTTLE_IDLE_MAX)
        p->neutral_seen=1u;
    if (p->powered && (!dashboard_valid || brake || throttle<=THROTTLE_IDLE_MAX)) {
        motor_pipeline_stop(p,!dashboard_valid?MOTOR_STOP_DASH:
                           brake?MOTOR_STOP_BRAKE:MOTOR_STOP_THROTTLE);
        return;
    }
    if (dashboard_valid) p->last_dashboard_ms=now_ms;
}

void motor_pipeline_watchdog(motor_pipeline_t *p, uint32_t now_ms,
                             uint32_t dash_timeout_ms) {
    if (p && p->powered &&
        (uint32_t)(now_ms-p->last_dashboard_ms)>dash_timeout_ms)
        motor_pipeline_stop(p,MOTOR_STOP_DASH);
}

uint8_t motor_pipeline_on_adc(motor_pipeline_t *p, foc_adc_sample_t *s,
                              uint16_t vbus_adc) {
    if (!p || !p->powered) return 0u;
    if (!s || !p->adc_qualified || p->foc->fault ||
        p->foc->state==SENSORLESS_FAULT ||
        s->sequence!=p->next_sequence) {
        motor_pipeline_stop(p,MOTOR_STOP_ADC); return 0u;
    }
    foc_adc_result_t current;
    if (!foc_adc_contract_complete(&p->adc,s,&current)) {
        motor_pipeline_stop(p,MOTOR_STOP_ADC); return 0u;
    }
    p->last_phase=current.counts;
    /* Enforce the raw-count hard trip BEFORE FOC and TIM1 CCR writes.
     * Not a calibrated ampere limit and not a substitute for hardware BKIN. */
    int32_t a=current.counts.ia,b=current.counts.ib,c=current.counts.ic;
    if(a<0)a=-a; if(b<0)b=-b; if(c<0)c=-c;
    if(a>p->current_hard_limit_counts || b>p->current_hard_limit_counts ||
       c>p->current_hard_limit_counts) {
        motor_pipeline_stop(p,MOTOR_STOP_FAULT); return 0u;
    }
    if (++p->sample_divider>=2u) {
        p->sample_divider=0u;
        const phase_current_counts_t phase={current.counts.ia,current.counts.ib,current.counts.ic};
        sensorless_control_step(p->foc,phase,vbus_adc,p->arr,1u);
        if (p->foc->fault || !p->foc->drive_request ||
            p->foc->state==SENSORLESS_FAULT ||
            p->foc->state==SENSORLESS_STOP) {
            motor_pipeline_stop(p,MOTOR_STOP_FAULT); return 0u;
        }
        if (!p->powered) return 0u; /* asynchronous brake/timeout won */
        if (p->foc->ccr1>p->arr || p->foc->ccr2>p->arr || p->foc->ccr3>p->arr ||
            !p->io.set_pwm(p->io_ctx,p->foc->ccr1,p->foc->ccr2,p->foc->ccr3)) {
            motor_pipeline_stop(p,MOTOR_STOP_PWM); return 0u;
        }
    }
    /* The sector used for the *next* sample belongs to the PWM vector
     * already written, not the ADC sample we just consumed. */
    if (!p->powered) return 0u; /* no new ADC plan after gate kill */
    const uint8_t next_sector=stock_current_sector_from_ab(
        p->foc->v_alpha_prev,p->foc->v_beta_prev);
    ++p->next_sequence;
    if (!foc_adc_contract_prepare(&p->adc,next_sector,p->next_sequence,p->offsets,1u) ||
        !p->io.set_adc_pair(p->io_ctx,p->adc.expected_jsqr1,p->adc.expected_jsqr2)) {
        motor_pipeline_stop(p,MOTOR_STOP_ADC); return 0u;
    }
    if (!p->powered) return 0u;
    p->last_sector=next_sector;
    return 1u;
}
