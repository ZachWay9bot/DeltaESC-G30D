#include "foc_adc_motor_handoff.h"

static uint8_t deny(sensorless_control_t *c, foc_gate_kill_fn kill, void *ctx) {
    if (kill) kill(ctx);
    if (c) sensorless_control_stop(c);
    return 0u;
}

uint8_t foc_adc_motor_handoff_step(sensorless_control_t *c,
                                   const foc_adc_result_t *r,
                                   uint16_t vbus_adc,
                                   uint16_t pwm_arr,
                                   uint8_t power_armed,
                                   uint8_t sample_hw_qualified,
                                   foc_gate_kill_fn gate_kill,
                                   void *context) {
    /* No callback == no physically defined coast. Even mathematically
       neutral CCRs must NEVER be treated as gate-off. */
    if (!c || !r || !r->valid || !gate_kill || !power_armed ||
        !sample_hw_qualified || !pwm_arr || !c->drive_request || c->fault ||
        c->state == SENSORLESS_FAULT || c->state == SENSORLESS_STOP)
        return deny(c, gate_kill, context);
    const phase_current_counts_t current={r->counts.ia,r->counts.ib,r->counts.ic};
    sensorless_control_step(c,current,vbus_adc,pwm_arr,1u);
    /* A state-machine fault may appear DURING this tick. Gate kill is
       requested before returning, not in the next ADC interrupt. */
    if (c->fault || c->state == SENSORLESS_FAULT ||
        c->state == SENSORLESS_STOP || !c->drive_request)
        return deny(c, gate_kill, context);
    return 1u;
}
