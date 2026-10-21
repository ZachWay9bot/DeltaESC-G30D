#pragma once
#include <stdint.h>
#include "foc_adc_contract.h"
#include "sensorless_control.h"

/* One owner for ADC -> reconstructed phase currents -> sensorless FOC/SVPWM
 * -> TIM1 CCR preloads -> physically isolated gate channels.
 * This interface models the entire path; hardware acceptance is an external
 * mandatory qualification, not inferred from source-code compilation.
 */
typedef struct {
    uint8_t (*arm)(void *ctx);
    void (*gate_off)(void *ctx);
    uint8_t (*set_adc_pair)(void *ctx, uint32_t jsqr1, uint32_t jsqr2);
    uint8_t (*set_pwm)(void *ctx, uint16_t c1, uint16_t c2, uint16_t c3);
    /* Optional normal coast. Faults and lost input always use gate_off. */
    void (*coast)(void *ctx);
} motor_pipeline_io_t;

enum {
    MOTOR_STOP_NONE=0u, MOTOR_STOP_THROTTLE=1u, MOTOR_STOP_BRAKE=2u,
    MOTOR_STOP_DASH=3u, MOTOR_STOP_ADC=4u, MOTOR_STOP_FAULT=5u,
    MOTOR_STOP_PWM=6u, MOTOR_STOP_QUALIFICATION=7u
};
typedef struct {
    foc_adc_contract_t adc;
    motor_pipeline_io_t io;
    sensorless_control_t *foc;
    void *io_ctx;
    uint32_t next_sequence;
    uint32_t last_dashboard_ms;
    uint16_t arr;
    uint16_t current_hard_limit_counts;
    uint16_t offsets[3];
    stock_phase_currents_t last_phase;
    uint8_t powered;
    uint8_t sample_divider;
    uint8_t last_sector;
    uint8_t last_stop_reason;
    uint8_t adc_qualified;
    uint8_t fault_latched;
    uint8_t neutral_seen;

    /* Persistent snapshot captured after physical gate-off but before the
     * live FOC object is reset to STOP. Read-only diagnostic evidence. */
    uint8_t stop_foc_state;
    uint8_t stop_foc_fault;
    int16_t stop_phase_error;
    int16_t stop_observer_step;
    uint16_t stop_observer_phase;
    uint16_t stop_control_phase;
    int16_t stop_id;
    int16_t stop_iq;
    uint16_t stop_clamp_delta;
    uint16_t stop_rolling_good_ticks;
    uint16_t stop_rolling_iq_ticks;
} motor_pipeline_t;

void motor_pipeline_init(motor_pipeline_t *p, sensorless_control_t *foc,
                         motor_pipeline_io_t io, void *io_ctx, uint16_t arr,
                         uint16_t hard_current_limit_counts);
/* Caller supplies independently-established power-stage and ADC-window
 * qualification. No lab safety claim is made by this Boolean interface. */
uint8_t motor_pipeline_start(motor_pipeline_t *p, const uint16_t offsets[3],
                             uint8_t gate_qualified, uint8_t adc_qualified,
                             uint8_t throttle, uint8_t brake,
                             uint8_t dashboard_valid, uint32_t now_ms);
uint8_t motor_pipeline_start_rolling(motor_pipeline_t *p,const uint16_t offsets[3],
                             uint8_t gate_qualified,uint8_t adc_qualified,
                             uint8_t throttle,uint8_t brake,uint8_t dashboard_valid,
                             uint32_t now_ms,uint16_t phase_u16,int16_t electrical_step_u16);
/* gate_off is performed BEFORE any software neutral-vector calculation. */
void motor_pipeline_stop(motor_pipeline_t *p, uint8_t reason);
void motor_pipeline_input(motor_pipeline_t *p, uint8_t throttle,
                          uint8_t brake, uint8_t dashboard_valid, uint32_t now_ms);
void motor_pipeline_watchdog(motor_pipeline_t *p, uint32_t now_ms,
                             uint32_t dash_timeout_ms);
/* Must be called exactly once for each completed TIM1-triggered injected
 * ADC1+ADC2 one-rank event; never from the two-rank diagnostic mode. */
uint8_t motor_pipeline_on_adc(motor_pipeline_t *p, foc_adc_sample_t *sample,
                              uint16_t vbus_adc);
