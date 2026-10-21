#pragma once
#include <stdint.h>
#include "foc_adc_contract.h"
#include "sensorless_control.h"

/* OFFLINE bridge: does NOT touch TIM1 or GPIO. `gate_kill` must eventually
 * be a physically verified hardware shutdown routine, not 50%-duty PWM.
 * A missing gate_kill callback is always a refusal to command torque. */
typedef void (*foc_gate_kill_fn)(void *context);

uint8_t foc_adc_motor_handoff_step(sensorless_control_t *control,
                                   const foc_adc_result_t *sample,
                                   uint16_t bus_voltage_adc,
                                   uint16_t pwm_arr,
                                   uint8_t power_armed,
                                   uint8_t physical_sample_qualified,
                                   foc_gate_kill_fn gate_kill,
                                   void *context);
