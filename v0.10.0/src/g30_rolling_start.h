#pragma once
#include <stdint.h>
#include "g30_rolling_bemf.h"

typedef enum {
    G30_RS_WAIT_NEUTRAL=0,
    G30_RS_WAIT_ROLL=1,
    G30_RS_VERIFY=2,
    G30_RS_RAMP=3,
    G30_RS_CLOSED=4,
    G30_RS_FAULT=5
} g30_rolling_start_state_t;

typedef enum {
    G30_RS_ABORT_NONE=0,
    G30_RS_ABORT_INPUT=1,
    G30_RS_ABORT_ROLL_LOST=2,
    G30_RS_ABORT_OBSERVER=3,
    G30_RS_ABORT_MAGNITUDE=4,
    G30_RS_ABORT_PHASE=5,
    G30_RS_ABORT_PHASE_STEP=6,
    G30_RS_ABORT_CLAMP=7,
    G30_RS_ABORT_TIMEOUT=8
} g30_rolling_start_abort_t;

typedef struct {
    uint16_t throttle_idle_max;
    uint16_t verify_ticks;
    uint16_t verify_timeout_ticks;
    uint16_t ramp_ticks;
    uint16_t target_iq_counts;
    uint16_t max_now_phase_error;
    uint16_t max_avg_phase_error;
    uint16_t max_abs_phase_step;
    uint16_t max_positive_phase_step;
} g30_rolling_start_config_t;

typedef struct {
    uint8_t dash_valid;
    uint8_t brake;
    uint8_t fault;
    uint8_t motor_params_valid;
    uint16_t throttle;
    uint8_t rolling_ready;
    uint16_t rolling_phase;
    int16_t rolling_step;
    uint8_t observer_valid;
    uint16_t observer_phase;
    int16_t observer_step;
    uint32_t flux_magnitude_uwb;
    uint32_t flux_target_uwb;
    uint16_t observer_clamp_delta;
} g30_rolling_start_input_t;

typedef struct {
    uint8_t state;
    uint8_t abort_reason;
    uint8_t neutral_seen;
    uint8_t seed_requested;
    uint8_t closed_loop_allowed;
    uint16_t iq_ref_counts;
    uint16_t seed_phase;
    int16_t seed_step;
    uint16_t verify_good_ticks;
    uint16_t state_ticks;
    uint32_t phase_error_abs_sum;
} g30_rolling_start_t;

void g30_rolling_start_default_config(g30_rolling_start_config_t *c);
void g30_rolling_start_init(g30_rolling_start_t *s);
void g30_rolling_start_reset(g30_rolling_start_t *s);
void g30_rolling_start_step(g30_rolling_start_t *s,const g30_rolling_start_config_t *c,
                            const g30_rolling_start_input_t *in);
