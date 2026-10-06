#pragma once
#include <stdint.h>

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
    SENSORLESS_FAULT = 5
} sensorless_state_id_t;

typedef struct {
    int64_t x_alpha;
    int64_t x_beta;
    int32_t e_alpha;
    int32_t e_beta;
    int32_t i_alpha_prev;
    int32_t i_beta_prev;
    int32_t v_alpha_prev;
    int32_t v_beta_prev;

    uint16_t observer_phase;
    uint16_t control_phase;
    uint16_t openloop_phase;
    int16_t phase_error;

    int32_t id;
    int32_t iq;
    int32_t id_int;
    int32_t iq_int;

    uint32_t flux_sq;
    uint16_t state_ticks;
    uint16_t lock_ticks;
    uint16_t lost_ticks;
    uint16_t openloop_step;

    uint16_t ccr1;
    uint16_t ccr2;
    uint16_t ccr3;

    uint8_t drive_request;
    uint8_t observer_valid;
    uint8_t state;
    uint8_t fault;
} sensorless_control_t;

void sensorless_control_init(sensorless_control_t *s);
void sensorless_control_set_drive(sensorless_control_t *s, uint8_t enable);
void sensorless_control_stop(sensorless_control_t *s);
void sensorless_control_step(sensorless_control_t *s,
                             phase_current_counts_t current,
                             uint16_t vbus_adc,
                             uint16_t pwm_arr,
                             uint8_t power_armed);
const char *sensorless_state_name(uint8_t state);
