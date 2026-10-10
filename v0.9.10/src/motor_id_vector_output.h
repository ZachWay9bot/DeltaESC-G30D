/* v0.9.10: exclusive voltage-vector bridge for automatic Motor Detect.
 * Uses the SAME SVPWM as normal DeltaESC FOC. No real current/flux feedback
 * is claimed by this output-only driver. GPL-3.0-or-later. */
#pragma once
#include <stdint.h>

typedef struct {
    /* Prepare ADC1/ADC2 sector channel pair before the next PWM vector. */
    uint8_t (*adc_pair)(void *ctx,uint32_t jsqr1,uint32_t jsqr2);
    uint8_t (*pwm_ccr)(void *ctx,uint16_t c1,uint16_t c2,uint16_t c3);
    uint8_t (*arm)(void *ctx);
    void (*gate_off)(void *ctx);
} motor_id_vector_io_t;

typedef struct {
    motor_id_vector_io_t io;
    void *ctx;
    uint8_t owning;
    uint8_t bridge_armed;
    uint8_t sector;
    uint32_t accepted;
    uint32_t rejected;
} motor_id_vector_output_t;

void motor_id_vector_output_init(motor_id_vector_output_t *v,
                                 motor_id_vector_io_t io,void *ctx);
/* Positive mV means stator alpha/beta vector at electrical angle_u16.
 * The caller MUST supply a fresh measured bus voltage and independent
 * current/timing qualification; a rejected command disables the bridge. */
uint8_t motor_id_vector_output_request(motor_id_vector_output_t *v,
                                       int32_t amplitude_mv,uint16_t angle_u16,
                                       uint32_t measured_vbus_mv,uint16_t arr,
                                       uint8_t qualified,uint8_t drive_owns_pwm,
                                       uint8_t brake_or_fault);
void motor_id_vector_output_stop(motor_id_vector_output_t *v);