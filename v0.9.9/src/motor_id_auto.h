/* DeltaESC v0.9.9: automatic R/L/flux excitation and sampling sequencer.
 * Software source only. A hardware integration MUST provide a qualified,
 * independently fail-closed output adapter and calibrated feedback.
 * GPL-3.0-or-later; motor_id formula/ABI from existing DeltaESC.
 */
#pragma once
#include <stdint.h>
#include "motor_id.h"
#include "motor_config_txn.h"

typedef enum {
    ID_AUTO_IDLE=0, ID_AUTO_R_SETTLE, ID_AUTO_R_COLLECT,
    ID_AUTO_L_REST, ID_AUTO_L_PULSE,
    ID_AUTO_FLUX_SPIN, ID_AUTO_FLUX_COAST,
    ID_AUTO_COMPLETE, ID_AUTO_FAULT
} motor_id_auto_state_t;

typedef struct {
    /* These are measured, not inferred from the requested PWM command. */
    uint32_t command_epoch;  /* ADC event belongs to current vector or coast */
    int32_t applied_voltage_mv;
    int32_t phase_current_ma;
    uint32_t measured_bemf_mv;
    uint32_t measured_electrical_speed_mrad_s;
    uint8_t adc_valid;
    uint8_t voltage_valid;
    uint8_t bemf_valid;
    uint8_t speed_valid;
    uint8_t throttle_idle;
    uint8_t brake_active;
    uint8_t fault_active;
    uint8_t stop_requested;
} motor_id_auto_feedback_t;

typedef struct {
    /* Driver implements a voltage vector, including ADC sector selection.
     * It must return zero on a rejected command (causing immediate coast).
     * This callback NEVER assumes a requested mV equals measured mV. */
    uint8_t (*vector_mv)(void *ctx, int32_t amplitude_mv, uint16_t angle_u16);
    /* Required. True bridge gate isolation, NOT neutral/50% duty. */
    void (*gate_off)(void *ctx);
    /* Optional notification after atomic RAM commit (e.g. BLE cache update). */
    void (*result_applied)(void *ctx, const motor_id_result_t *result);
} motor_id_auto_io_t;

typedef struct {
    motor_id_t id;
    motor_id_auto_io_t io;
    void *io_ctx;
    sensorless_control_t *control;
    motor_config_txn_t *config;
    uint32_t last_event_us;
    uint32_t command_at_us;
    uint32_t command_epoch;
    uint32_t stage_started_us;
    uint32_t next_sample_us;
    uint32_t seq;
    int32_t previous_current_ma;
    uint16_t flux_angle;
    uint16_t r_request_mv;
    uint8_t state;
    uint8_t output_active;
    uint8_t fault;
} motor_id_auto_t;

enum {
    ID_AUTO_FAULT_DRIVER=1u, ID_AUTO_FAULT_SENSOR=2u,
    ID_AUTO_FAULT_WATCHDOG=4u, ID_AUTO_FAULT_SAFETY=8u,
    ID_AUTO_FAULT_COMMIT=16u
};

void motor_id_auto_init(motor_id_auto_t *a, motor_id_auto_io_t io,
                        void *ctx, sensorless_control_t *control);
/* Begin only when motor_id qualifications are fully met AND control is idle.
 * No hardware is energized by init or a rejected begin. */
/* Optional RAM-only atomic application to active config + observer. */
void motor_id_auto_bind_config(motor_id_auto_t *a, motor_config_txn_t *config);
uint8_t motor_id_auto_begin(motor_id_auto_t *a,
                            const motor_id_qualification_t *q,
                            uint32_t now_us);
/* One call for each coherently sampled ADC event; no fabricated measurements.
 * The function automatically requests R/L excitation, flux rotation/coast,
 * accepts each real measurement, stops outputs, and commits the result.
 * Returns 1 while running/complete; 0 if aborted/faulted. */
uint8_t motor_id_auto_tick(motor_id_auto_t *a, uint32_t now_us,
                           const motor_id_auto_feedback_t *fb);
void motor_id_auto_abort(motor_id_auto_t *a);