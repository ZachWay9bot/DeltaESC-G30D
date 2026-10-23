/* Motor-ID driven flux observer. Uses physical two-shunt current sampling
 * and effective TIM1 modulation; never infers rotor speed from command angle.
 * VESC-style driven-flux relation E = V - R*I - L*dI/dt, lambda = |E|/omega.
 */
#pragma once
#include <stdint.h>
#include "motor_id_auto.h"

typedef struct {
    int32_t previous_ia_ma,previous_ib_ma;
    int32_t ea_filtered_mv,eb_filtered_mv;
    uint32_t previous_us,phase_at_us;
    uint16_t phase_at_window;
    uint32_t valid_windows,invalid_windows;
    uint8_t primed,filtered,phase_ready;
} motor_id_flux_feedback_t;

void motor_id_flux_feedback_reset(motor_id_flux_feedback_t *s);
/* Called only while PWM is physically active and ADC vector/epoch is valid.
 * The rotor speed estimate uses the rotation of inferred back-EMF, not the
 * open-loop commanded field angle or commanded angular speed. */
void motor_id_flux_feedback_observe(motor_id_flux_feedback_t *s,
    const motor_id_t *id,uint32_t now_us,motor_id_auto_feedback_t *fb);
