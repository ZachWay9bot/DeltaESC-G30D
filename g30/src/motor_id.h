#pragma once
#include <stdint.h>
#include "sensorless_control.h"

/* v0.9.3: Hardware-neutral motor parameter identification from externally
 * calibrated measurement tuples. Does NOT command PWM, energize phases, or
 * infer electrical gain/polarity. Admission requires independent evidence.
 * Quantities are millivolt, milliampere, microseconds, mrad/s, microohm,
 * nanohenry and micro-weber, all 32-bit integer at the external interface.
 */
typedef enum {
    MOTOR_ID_IDLE=0, MOTOR_ID_RESISTANCE=1, MOTOR_ID_INDUCTANCE=2,
    MOTOR_ID_FLUX=3, MOTOR_ID_READY=4, MOTOR_ID_FAULT=5
} motor_id_stage_t;

enum {
    MOTOR_ID_BAD_QUALIFICATION=1u<<0,
    MOTOR_ID_BAD_SAMPLE=1u<<1,
    MOTOR_ID_TIMEOUT=1u<<2,
    MOTOR_ID_INCONSISTENT=1u<<3,
    MOTOR_ID_RANGE=1u<<4,
    MOTOR_ID_SEQUENCE=1u<<5,
    MOTOR_ID_ABORT=1u<<6
};

typedef struct {
    uint8_t gate_shutdown_verified;      /* independent physical kill path */
    uint8_t current_scale_verified;      /* ADC raw -> signed mA */
    uint8_t sample_timing_verified;      /* ADC and PWM event alignment */
    uint8_t bus_sense_verified;          /* actual applied phase voltage */
    uint8_t overcurrent_protection_verified;
    uint8_t wheel_unloaded;
    /* Explicitly experimental bench: no claim of verified mA/count or
     * analogue sample aperture. Requires a separate raw-ADC fault cutoff. */
    uint8_t bench_provisional_scale;
    uint8_t bench_adc_runtime_guard;
} motor_id_qualification_t;

typedef struct {
    uint32_t sequence;            /* strictly contiguous, wraps */
    uint32_t elapsed_us;          /* elapsed since previous tuple */
    uint8_t stage;                /* must match requested stage */
    uint8_t brake_active;
    uint8_t stop_requested;
    uint8_t fault_active;
    uint8_t samples_synchronized;
    int32_t voltage_mv;          /* actual phase voltage (signed) */
    int32_t current_ma;          /* actual phase current (signed) */
    int32_t previous_current_ma; /* sampled at the previous timestamp */
    uint32_t bemf_mv;            /* calibrated phase back-EMF */
    uint32_t electrical_speed_mrad_s; /* not mechanical speed */
} motor_id_sample_t;

typedef struct {
    uint32_t r_uohm;
    uint32_t l_nh;
    uint32_t flux_uwb;
    uint32_t samples_r;
    uint32_t samples_l;
    uint32_t samples_flux;
    uint8_t valid;
} motor_id_result_t;

typedef struct {
    motor_id_result_t result;
    uint64_t sum;
    uint32_t samples;
    uint32_t last_seq;
    uint32_t elapsed_stage_us;
    uint32_t min_value;
    uint32_t max_value;
    uint8_t seen_seq;
    uint8_t target_samples_r, target_samples_l, target_samples_flux;
    uint8_t state;
    uint8_t fault;
} motor_id_t;

void motor_id_init(motor_id_t *m);
/* Automatic VESC-style sequence uses longer stage averages. The generic
 * measurement ABI retains the original eight-sample default. */
uint8_t motor_id_set_sample_targets(motor_id_t *m, uint8_t r, uint8_t l, uint8_t flux);
/* No actuation. Returns false if the safety qualifications are absent. */
uint8_t motor_id_begin(motor_id_t *m, const motor_id_qualification_t *q);
/* Returns 1 for accepted tuple, 0 on fault/refusal. */
uint8_t motor_id_accept(motor_id_t *m, const motor_id_sample_t *s);
/* Returns true ONLY on completion of all three stages. */
uint8_t motor_id_commit(const motor_id_t *m, sensorless_control_t *c);
/* Abort always zeroes parameter validity; an explicit new begin is required. */
void motor_id_abort(motor_id_t *m);
