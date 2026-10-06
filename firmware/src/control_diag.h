#pragma once
#include <stdint.h>

typedef struct {
    int32_t x_alpha;
    int32_t x_beta;
    int32_t i_alpha_prev;
    int32_t i_beta_prev;
    uint16_t phase;
    int32_t id_int;
    int32_t iq_int;
    int32_t v_alpha_prev;
    int32_t v_beta_prev;
    uint16_t ccr1;
    uint16_t ccr2;
    uint16_t ccr3;
} control_diag_state_t;

typedef struct {
    int16_t ia;
    int16_t ib;
    int16_t ic;
} phase_current_counts_t;

void control_diag_init(control_diag_state_t *s);
void control_diag_step(control_diag_state_t *s, phase_current_counts_t i, uint16_t pwm_arr);
