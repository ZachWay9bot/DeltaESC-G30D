/*
 * DeltaESC-G30D v0.5 sensorless control core.
 *
 * Observer structure is adapted from the fixed-point Sensorless_VESC work in
 * EBiCS/EBiCS_Firmware, which in turn adapts the nonlinear VESC flux observer.
 * This file is GPL-3.0-or-later and must remain under the project license.
 *
 * IMPORTANT: the default motor constants below are bring-up values, not
 * validated G30 motor identification results. The state machine requires
 * observer agreement before handover and faults instead of forcing takeover.
 */
#include "sensorless_control.h"

static const int16_t sin_lut_q15[256] = {
#include "sin_q15.inc"
};

#ifndef SENSORLESS_MOTOR_R_PARAM
#define SENSORLESS_MOTOR_R_PARAM 90LL
#endif
#ifndef SENSORLESS_MOTOR_L_PARAM
#define SENSORLESS_MOTOR_L_PARAM 10LL
#endif
#ifndef SENSORLESS_FLUX_LINKAGE
#define SENSORLESS_FLUX_LINKAGE 1800LL
#endif
#ifndef SENSORLESS_GAMMA_SHIFT
#define SENSORLESS_GAMMA_SHIFT 9u
#endif
#ifndef SENSORLESS_DT_SHIFT
/* EBiCS uses 13 at about 16 kHz. 4 kHz needs about 4x integration step. */
#define SENSORLESS_DT_SHIFT 11u
#endif
#ifndef SENSORLESS_PHASE_OFFSET_U16
#define SENSORLESS_PHASE_OFFSET_U16 0u
#endif

/* SmartESC G30 hardware data: 2 mOhm shunt, gain 8, 3.3 V / 12-bit ADC.
 * 1 ADC count is about 50.35 mA. Keep Q8 units to avoid float in the ISR. */
#define CURRENT_MA_PER_COUNT_Q8 12890LL

/* Conservative bring-up estimate for the PA1 bus divider used by this target.
 * This is intentionally isolated here because it must be calibrated on hardware. */
#define VBUS_MV_PER_COUNT_Q8 3584LL

#define CONTROL_HZ 4000u
#define ALIGN_TICKS 800u
#define OPEN_LOOP_MIN_TICKS 800u
#define OPEN_LOOP_TIMEOUT_TICKS 8000u
#define HANDOVER_TICKS 1600u
#define LOCK_REQUIRED_TICKS 240u
#define OBS_LOST_TICKS 240u

#define ALIGN_IQ_COUNTS 20
#define RUN_IQ_COUNTS 30
#define MAX_CURRENT_REF_COUNTS 40

#define PI_KP_Q15 5200
#define PI_KI_Q15 220
#define PI_INT_LIMIT 2600
#define MOD_LIMIT 2400

#define OPENLOOP_STEP_START 32u
#define OPENLOOP_STEP_END 240u

#define PHASE_LOCK_LIMIT_U16 5461 /* 30 electrical degrees */

static int32_t clamp32(int32_t x, int32_t lo, int32_t hi) {
    return x < lo ? lo : (x > hi ? hi : x);
}

static int64_t clamp64(int64_t x, int64_t lo, int64_t hi) {
    return x < lo ? lo : (x > hi ? hi : x);
}

static uint16_t fast_atan2_u16(int32_t y, int32_t x) {
    if ((x | y) == 0) return 0;
    uint32_t ax = (uint32_t)(x < 0 ? -x : x);
    uint32_t ay = (uint32_t)(y < 0 ? -y : y);
    uint32_t base;

    if (ax >= ay) {
        uint32_t r = ax ? (uint32_t)(((uint64_t)ay << 14) / ax) : 0u;
        base = (r * 8192u) >> 14;
    } else {
        uint32_t r = ay ? (uint32_t)(((uint64_t)ax << 14) / ay) : 0u;
        base = 16384u - ((r * 8192u) >> 14);
    }

    if (x >= 0 && y >= 0) return (uint16_t)base;
    if (x < 0 && y >= 0) return (uint16_t)(32768u - base);
    if (x < 0 && y < 0) return (uint16_t)(32768u + base);
    return (uint16_t)(65536u - base);
}

static int16_t sin_q15(uint16_t a) {
    return sin_lut_q15[a >> 8];
}

static int16_t cos_q15(uint16_t a) {
    return sin_lut_q15[(uint8_t)((a >> 8) + 64u)];
}

static int16_t phase_delta(uint16_t to, uint16_t from) {
    return (int16_t)(to - from);
}

static void zero_vector(sensorless_control_t *s, uint16_t pwm_arr) {
    uint16_t mid = (uint16_t)((pwm_arr + 1u) >> 1);
    s->ccr1 = mid;
    s->ccr2 = mid;
    s->ccr3 = mid;
    s->v_alpha_prev = 0;
    s->v_beta_prev = 0;
}

static void observer_reset(sensorless_control_t *s) {
    s->x_alpha = 0;
    s->x_beta = 0;
    s->e_alpha = 0;
    s->e_beta = 0;
    s->i_alpha_prev = 0;
    s->i_beta_prev = 0;
    s->v_alpha_prev = 0;
    s->v_beta_prev = 0;
    s->observer_phase = 0;
    s->flux_sq = 0;
    s->observer_valid = 0;
    s->lock_ticks = 0;
    s->lost_ticks = 0;
}

static void observer_update(sensorless_control_t *s,
                            int32_t i_alpha_counts,
                            int32_t i_beta_counts,
                            uint16_t vbus_adc) {
    const int64_t l = (3LL * SENSORLESS_MOTOR_L_PARAM) >> 1;
    const int64_t r = (3LL * SENSORLESS_MOTOR_R_PARAM) >> 1;
    const int64_t lambda = SENSORLESS_FLUX_LINKAGE;
    const int64_t lambda_sq = lambda * lambda;

    int64_t i_alpha = (int64_t)i_alpha_counts * CURRENT_MA_PER_COUNT_Q8;
    int64_t i_beta = (int64_t)i_beta_counts * CURRENT_MA_PER_COUNT_Q8;
    int64_t vbus_q8 = (int64_t)vbus_adc * VBUS_MV_PER_COUNT_Q8;

    int64_t v_alpha = ((int64_t)s->v_alpha_prev * vbus_q8) >> 15;
    int64_t v_beta = ((int64_t)s->v_beta_prev * vbus_q8) >> 15;

    int64_t l_ia = (l * i_alpha) >> 16;
    int64_t l_ib = (l * i_beta) >> 16;
    int64_t r_ia = (r * i_alpha) >> 9;
    int64_t r_ib = (r * i_beta) >> 9;

    int64_t e_alpha = s->x_alpha - l_ia;
    int64_t e_beta = s->x_beta - l_ib;

    e_alpha = clamp64(e_alpha, -65536LL, 65536LL);
    e_beta = clamp64(e_beta, -65536LL, 65536LL);

    int64_t err = lambda_sq - (e_alpha * e_alpha + e_beta * e_beta);
    err = clamp64(err, -lambda_sq, lambda_sq);

    int64_t x1_dot = -r_ia + v_alpha + ((e_alpha * err) >> SENSORLESS_GAMMA_SHIFT);
    int64_t x2_dot = -r_ib + v_beta + ((e_beta * err) >> SENSORLESS_GAMMA_SHIFT);

    s->x_alpha = clamp64(s->x_alpha + (x1_dot >> SENSORLESS_DT_SHIFT), -(1LL << 24), (1LL << 24));
    s->x_beta = clamp64(s->x_beta + (x2_dot >> SENSORLESS_DT_SHIFT), -(1LL << 24), (1LL << 24));

    e_alpha = clamp64(s->x_alpha - l_ia, -1073741823LL, 1073741823LL);
    e_beta = clamp64(s->x_beta - l_ib, -1073741823LL, 1073741823LL);
    s->e_alpha = (int32_t)e_alpha;
    s->e_beta = (int32_t)e_beta;

    int64_t flux_sq64 = e_alpha * e_alpha + e_beta * e_beta;
    s->flux_sq = flux_sq64 > 0xFFFFFFFFLL ? 0xFFFFFFFFu : (uint32_t)flux_sq64;

    int64_t valid_lo = lambda_sq >> 2;
    int64_t valid_hi = lambda_sq << 2;
    s->observer_valid = (flux_sq64 >= valid_lo && flux_sq64 <= valid_hi) ? 1u : 0u;
    s->observer_phase = (uint16_t)(fast_atan2_u16(-s->e_beta, s->e_alpha) +
                                   (uint16_t)SENSORLESS_PHASE_OFFSET_U16);
}

static void current_control(sensorless_control_t *s,
                            int32_t i_alpha,
                            int32_t i_beta,
                            uint16_t phase,
                            int32_t iq_ref,
                            uint16_t pwm_arr) {
    int32_t sn = sin_q15(phase);
    int32_t cs = cos_q15(phase);

    s->id = (int32_t)(((int64_t)i_alpha * cs + (int64_t)i_beta * sn) >> 15);
    s->iq = (int32_t)((-(int64_t)i_alpha * sn + (int64_t)i_beta * cs) >> 15);

    iq_ref = clamp32(iq_ref, -MAX_CURRENT_REF_COUNTS, MAX_CURRENT_REF_COUNTS);
    int32_t ed = -s->id;
    int32_t eq = iq_ref - s->iq;

    s->id_int = clamp32(s->id_int + ((ed * PI_KI_Q15) >> 15), -PI_INT_LIMIT, PI_INT_LIMIT);
    s->iq_int = clamp32(s->iq_int + ((eq * PI_KI_Q15) >> 15), -PI_INT_LIMIT, PI_INT_LIMIT);

    int32_t vd = clamp32(((ed * PI_KP_Q15) >> 15) + s->id_int, -MOD_LIMIT, MOD_LIMIT);
    int32_t vq = clamp32(((eq * PI_KP_Q15) >> 15) + s->iq_int, -MOD_LIMIT, MOD_LIMIT);

    int32_t v_alpha = (int32_t)(((int64_t)vd * cs - (int64_t)vq * sn) >> 15);
    int32_t v_beta = (int32_t)(((int64_t)vd * sn + (int64_t)vq * cs) >> 15);
    s->v_alpha_prev = v_alpha;
    s->v_beta_prev = v_beta;

    const int32_t sqrt3_2_q15 = 28378;
    int32_t pa = v_alpha;
    int32_t pb = -(v_alpha >> 1) + (int32_t)(((int64_t)v_beta * sqrt3_2_q15) >> 15);
    int32_t pc = -(v_alpha >> 1) - (int32_t)(((int64_t)v_beta * sqrt3_2_q15) >> 15);

    int32_t vmax = pa;
    int32_t vmin = pa;
    if (pb > vmax) vmax = pb;
    if (pc > vmax) vmax = pc;
    if (pb < vmin) vmin = pb;
    if (pc < vmin) vmin = pc;

    int32_t center = -((vmax + vmin) >> 1);
    pa = clamp32(pa + center, -MOD_LIMIT, MOD_LIMIT);
    pb = clamp32(pb + center, -MOD_LIMIT, MOD_LIMIT);
    pc = clamp32(pc + center, -MOD_LIMIT, MOD_LIMIT);

    int32_t da = clamp32(16384 + pa, 1024, 31744);
    int32_t db = clamp32(16384 + pb, 1024, 31744);
    int32_t dc = clamp32(16384 + pc, 1024, 31744);

    s->ccr1 = (uint16_t)(((uint32_t)da * pwm_arr) >> 15);
    s->ccr2 = (uint16_t)(((uint32_t)db * pwm_arr) >> 15);
    s->ccr3 = (uint16_t)(((uint32_t)dc * pwm_arr) >> 15);
}

void sensorless_control_init(sensorless_control_t *s) {
    s->drive_request = 0;
    s->state = SENSORLESS_STOP;
    s->fault = 0;
    s->state_ticks = 0;
    s->openloop_step = OPENLOOP_STEP_START;
    s->openloop_phase = 0;
    s->control_phase = 0;
    s->id = 0;
    s->iq = 0;
    s->id_int = 0;
    s->iq_int = 0;
    observer_reset(s);
    s->ccr1 = s->ccr2 = s->ccr3 = 0;
}

void sensorless_control_set_drive(sensorless_control_t *s, uint8_t enable) {
    if (!enable) {
        sensorless_control_stop(s);
        return;
    }
    if (!s->drive_request) {
        s->drive_request = 1;
        s->fault = 0;
        s->state = SENSORLESS_ALIGN;
        s->state_ticks = 0;
        s->openloop_step = OPENLOOP_STEP_START;
        s->openloop_phase = 0;
        s->control_phase = 0;
        s->id_int = 0;
        s->iq_int = 0;
        observer_reset(s);
    }
}

void sensorless_control_stop(sensorless_control_t *s) {
    s->drive_request = 0;
    s->state = SENSORLESS_STOP;
    s->state_ticks = 0;
    s->lock_ticks = 0;
    s->lost_ticks = 0;
    s->id_int = 0;
    s->iq_int = 0;
}

void sensorless_control_step(sensorless_control_t *s,
                             phase_current_counts_t current,
                             uint16_t vbus_adc,
                             uint16_t pwm_arr,
                             uint8_t power_armed) {
    int32_t i_alpha = current.ia;
    int32_t i_beta = (int32_t)(((int64_t)(current.ia + 2 * (int32_t)current.ib) * 18919) >> 15);

    observer_update(s, i_alpha, i_beta, vbus_adc);
    s->phase_error = phase_delta(s->observer_phase, s->openloop_phase);

    if (!power_armed || !s->drive_request) {
        s->state = SENSORLESS_STOP;
        s->state_ticks = 0;
        s->id_int = 0;
        s->iq_int = 0;
        zero_vector(s, pwm_arr);
        return;
    }

    s->state_ticks++;

    switch ((sensorless_state_id_t)s->state) {
    case SENSORLESS_STOP:
        s->state = SENSORLESS_ALIGN;
        s->state_ticks = 0;
        s->openloop_phase = 0;
        s->control_phase = 0;
        break;

    case SENSORLESS_ALIGN:
        s->control_phase = s->openloop_phase;
        if (s->state_ticks >= ALIGN_TICKS) {
            s->state = SENSORLESS_OPEN_LOOP;
            s->state_ticks = 0;
            s->lock_ticks = 0;
        }
        break;

    case SENSORLESS_OPEN_LOOP: {
        uint32_t ramp = s->state_ticks;
        if (ramp > CONTROL_HZ) ramp = CONTROL_HZ;
        s->openloop_step = (uint16_t)(OPENLOOP_STEP_START +
            ((OPENLOOP_STEP_END - OPENLOOP_STEP_START) * ramp) / CONTROL_HZ);
        s->openloop_phase = (uint16_t)(s->openloop_phase + s->openloop_step);
        s->control_phase = s->openloop_phase;

        int32_t pe = s->phase_error;
        if (pe < 0) pe = -pe;
        if (s->observer_valid && pe < PHASE_LOCK_LIMIT_U16 && s->state_ticks > OPEN_LOOP_MIN_TICKS) {
            if (s->lock_ticks < 0xFFFFu) s->lock_ticks++;
        } else {
            s->lock_ticks = 0;
        }

        if (s->lock_ticks >= LOCK_REQUIRED_TICKS) {
            s->state = SENSORLESS_HANDOVER;
            s->state_ticks = 0;
        } else if (s->state_ticks >= OPEN_LOOP_TIMEOUT_TICKS) {
            s->fault = 1;
            s->state = SENSORLESS_FAULT;
        }
        break;
    }

    case SENSORLESS_HANDOVER: {
        s->openloop_phase = (uint16_t)(s->openloop_phase + s->openloop_step);
        int16_t d = phase_delta(s->observer_phase, s->openloop_phase);
        uint32_t blend = s->state_ticks;
        if (blend > HANDOVER_TICKS) blend = HANDOVER_TICKS;
        s->control_phase = (uint16_t)(s->openloop_phase +
            (int32_t)(((int64_t)d * blend) / HANDOVER_TICKS));

        if (!s->observer_valid) {
            if (s->lost_ticks < 0xFFFFu) s->lost_ticks++;
        } else {
            s->lost_ticks = 0;
        }

        if (s->lost_ticks >= OBS_LOST_TICKS) {
            s->fault = 2;
            s->state = SENSORLESS_FAULT;
        } else if (s->state_ticks >= HANDOVER_TICKS) {
            s->state = SENSORLESS_CLOSED_LOOP;
            s->state_ticks = 0;
            s->lost_ticks = 0;
        }
        break;
    }

    case SENSORLESS_CLOSED_LOOP:
        s->control_phase = s->observer_phase;
        if (!s->observer_valid) {
            if (s->lost_ticks < 0xFFFFu) s->lost_ticks++;
        } else {
            s->lost_ticks = 0;
        }
        if (s->lost_ticks >= OBS_LOST_TICKS) {
            s->fault = 3;
            s->state = SENSORLESS_FAULT;
        }
        break;

    case SENSORLESS_FAULT:
    default:
        s->drive_request = 0;
        zero_vector(s, pwm_arr);
        return;
    }

    int32_t iq_ref = (s->state == SENSORLESS_ALIGN) ? ALIGN_IQ_COUNTS : RUN_IQ_COUNTS;
    current_control(s, i_alpha, i_beta, s->control_phase, iq_ref, pwm_arr);
}

const char *sensorless_state_name(uint8_t state) {
    switch ((sensorless_state_id_t)state) {
    case SENSORLESS_STOP: return "STOP";
    case SENSORLESS_ALIGN: return "ALIGN";
    case SENSORLESS_OPEN_LOOP: return "OPEN";
    case SENSORLESS_HANDOVER: return "HANDOVER";
    case SENSORLESS_CLOSED_LOOP: return "CLOSED";
    case SENSORLESS_FAULT: return "FAULT";
    default: return "?";
    }
}
