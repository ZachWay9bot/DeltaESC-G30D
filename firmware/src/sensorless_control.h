#pragma once
#include <stdint.h>
#include "vesc_ebics_flux.h"

typedef struct {
    int16_t ia;
    int16_t ib;
    int16_t ic;
} phase_current_counts_t;

typedef enum {
    SENSORLESS_STOP = 0,
    SENSORLESS_ALIGN = 1,
    SENSORLESS_OPEN_LOOP = 2,
    SENSORLESS_HANDOVER = 3,
    SENSORLESS_CLOSED_LOOP = 4,
    SENSORLESS_FAULT = 5,
    SENSORLESS_ROLL_VERIFY = 6
} sensorless_state_id_t;

typedef struct {
    /* Back-EMF observer state, q8 millivolts. */
    vesc_ebics_flux_t vesc_flux;
    int32_t bemf_alpha_q8;
    int32_t bemf_beta_q8;
    int32_t i_alpha_prev_ma;
    int32_t i_beta_prev_ma;
    int32_t v_alpha_prev;
    int32_t v_beta_prev;
    int16_t observer_step;
    uint16_t observer_phase_prev;

    uint16_t observer_phase;
    uint16_t control_phase;
    uint16_t openloop_phase;
    int16_t phase_error;

    int32_t id;
    int32_t iq;
    int32_t id_int;
    int32_t iq_int;

    uint32_t bemf_mv;
    uint16_t state_ticks;
    uint16_t lock_ticks;
    uint16_t lost_ticks;
    uint16_t openloop_step;
    int16_t rolling_step;
    uint16_t rolling_good_ticks;
    uint16_t rolling_iq_ticks;
    uint16_t rolling_clamp_start;
    uint32_t rolling_phase_error_sum;
    uint8_t rolling_start;

    uint16_t ccr1;
    uint16_t ccr2;
    uint16_t ccr3;
    uint16_t pwm_arr_last; /* retained to force immediate neutral duty on stop */

    uint32_t motor_r_uohm;
    uint32_t motor_l_nh;
    uint32_t motor_flux_uwb;
    uint16_t phase_offset_u16;
    uint16_t run_current_ma;
    int16_t run_iq_counts;

    int16_t current_sum_counts;
    uint16_t pi_kp_mod_per_count;
    uint16_t pi_ki_mod_per_count;

    uint8_t drive_request;
    uint8_t observer_valid;
    uint8_t motor_params_valid;
    uint8_t state;
    uint8_t fault;
} sensorless_control_t;

void sensorless_control_init(sensorless_control_t *s);
void sensorless_control_set_drive(sensorless_control_t *s, uint8_t enable);
/* FO70 kick-start: seed the observer from passive BEMF and enter zero-Iq
 * verification. Physical bridge arming remains owned by motor_pipeline. */
uint8_t sensorless_control_start_rolling(sensorless_control_t *s,
                                         uint16_t phase_u16,
                                         int16_t electrical_step_u16);
void sensorless_control_stop(sensorless_control_t *s);
void sensorless_control_set_phase_offset(sensorless_control_t *s, int16_t offset);
uint8_t sensorless_control_set_run_current_ma(sensorless_control_t *s, uint16_t current_ma);
uint8_t sensorless_control_set_motor_params(sensorless_control_t *s,
                                            uint32_t r_uohm,
                                            uint32_t l_nh,
                                            uint32_t flux_uwb);
int32_t sensorless_counts_to_ma(int32_t counts);
void sensorless_control_step(sensorless_control_t *s,
                             phase_current_counts_t current,
                             uint16_t vbus_adc,
                             uint16_t pwm_arr,
                             uint8_t power_armed);
const char *sensorless_state_name(uint8_t state);

/* Reuse the SAME fixed-point SVPWM implementation as drive control.
 * Stationary alpha/beta voltage in millivolts is represented by angle_u16
 * and amplitude_mv; no motor excitation or peripheral access here.
 * Reject overmodulation, invalid bus measurement and bad PWM apertures.
 */
typedef struct {
    uint16_t ccr1,ccr2,ccr3;
    int32_t alpha_q15,beta_q15;
} sensorless_id_pwm_t;
uint8_t sensorless_id_voltage_to_pwm(int32_t amplitude_mv,uint16_t angle_u16,
                                     uint32_t vbus_mv,uint16_t arr,
                                     sensorless_id_pwm_t *out);
