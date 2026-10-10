/* DeltaESC v0.9.11: coherent injected-ADC -> Motor-ID feedback.
 * TIM1 preloads are never treated as currently effective duty cycles.
 * Source-only until analogue calibration and aperture verified on hardware. */
#pragma once
#include <stdint.h>
#include "motor_id_auto.h"

typedef struct {
    uint16_t ccr[3];
    uint16_t angle_u16;
    uint8_t sector;
    uint32_t epoch;
} motor_id_pwm_frame_t;

typedef struct {
    motor_id_pwm_frame_t queued;
    motor_id_pwm_frame_t active;
    uint32_t active_update_cycle;
    uint32_t samples_accepted;
    uint32_t samples_rejected;
    uint8_t pending;
    uint8_t active_valid;
} motor_id_adc_feedback_t;

typedef struct {
    uint32_t now_cycles;
    uint32_t clock_hz;
    uint32_t vbus_age_ms;
    uint32_t vbus_mv;
    uint32_t jsqr1,jsqr2;
    uint16_t adc1_raw,adc2_raw;
    uint16_t offset[3];
    uint16_t adc2_ch4_offset,adc2_ch5_offset;
    uint16_t tim1_arr;
    uint8_t adc1_jeoc,adc2_jeoc;
    uint8_t dual_mode_valid,window_valid;
    uint8_t adc2_ch4_offset_valid,adc2_ch5_offset_valid;
    uint8_t current_scale_qualified;
    uint8_t timing_qualified;
} motor_id_adc_input_t;

void motor_id_adc_feedback_init(motor_id_adc_feedback_t *f);
/* From vector callback while interrupts are masked, AFTER CCR preloads write.
 * `epoch` is motor_id_auto.command_epoch + 1 (increment after callback). */
void motor_id_adc_feedback_queue(motor_id_adc_feedback_t *f,
    uint16_t c1,uint16_t c2,uint16_t c3,uint16_t angle,
    uint8_t sector,uint32_t epoch);
/* Called solely on a physical TIM1 update interrupt after preload transfer. */
void motor_id_adc_feedback_on_update(motor_id_adc_feedback_t *f,uint32_t cycle);
/* Immediate physical gate-off invalidates current effective PWM snapshot. */
void motor_id_adc_feedback_off(motor_id_adc_feedback_t *f);
/* Reconstructs REAL sampled ADC current, voltage from applied PWM and
 * sampled VBUS, electrical angle projection. Fails closed on invalid data. */
uint8_t motor_id_adc_feedback_capture(motor_id_adc_feedback_t *f,
    const motor_id_adc_input_t *in,motor_id_auto_feedback_t *out);
