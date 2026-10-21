/*
 * DeltaESC-G30D v0.7 motor core.
 *
 * Bring-up order is intentionally split by build:
 *   MOTOR_VOLTAGE_BENCH=1       low-voltage rotating vector, no current feedback
 *   SENSORLESS_CLOSED_LOOP_ALLOWED=0  open-loop current FOC only
 *   SENSORLESS_CLOSED_LOOP_ALLOWED=1  BEMF observer handover enabled
 *
 * The startup/state-machine structure follows the same principle used by VESC:
 * known-angle alignment -> forced rotating field -> observer qualification ->
 * blended handover -> observer closed loop. The first qualification observer
 * here is a physically-scaled back-EMF observer; it is deliberately easier to
 * validate on the G30 hardware than the old provisional fixed-point Ortega port.
 */
#include "sensorless_control.h"
#include "stock_current_frontend.h"

static const int16_t sin_lut_q15[256] = {
#include "sin_q15.inc"
};

#ifndef MOTOR_VOLTAGE_BENCH
#define MOTOR_VOLTAGE_BENCH 0
#endif
#ifndef VESC_EBICS_OBSERVER
#define VESC_EBICS_OBSERVER 1
#endif
#ifndef SENSORLESS_CLOSED_LOOP_ALLOWED
#define SENSORLESS_CLOSED_LOOP_ALLOWED 0
#endif

/* Phase-count conversion has ONE definition in stock_current_frontend.c. */
#define VBUS_MV_PER_COUNT_Q8     3584LL  /* provisional ~14.0 mV/count */
#define CONTROL_HZ               4000u
#define ALIGN_TICKS               800u   /* 200 ms */
#define OPEN_LOOP_MIN_TICKS       800u   /* 200 ms */
#define OPEN_LOOP_BENCH_TICKS   12000u   /* 3.0 s, then clean stop */
#define OPEN_LOOP_TIMEOUT_TICKS  8000u   /* 2.0 s to find observer lock */
#define HANDOVER_TICKS            800u   /* 200 ms */
#define LOCK_REQUIRED_TICKS       240u   /* 60 ms continuous lock */
#define OBS_LOST_TICKS            160u   /* 40 ms */
/* FO69 used 80 confidence evaluations at 1 kHz. DeltaESC evaluates at 4 kHz,
 * so preserve the ~80 ms quiet-verification time rather than merely 80 ticks. */
#define ROLL_VERIFY_TICKS           320u
#define ROLL_VERIFY_TIMEOUT_TICKS  1200u
#define ROLL_IQ_RAMP_TICKS         1000u
#define ROLL_MAX_IQ_COUNTS           12
#define ROLL_PHASE_NOW_MAX_U16     8192u
#define ROLL_PHASE_AVG_MAX_U16     5461u
#define ROLL_PHASE_OVER90_U16     16384u
#define ROLL_STEP_ABS_MAX_U16       1024u
#define ROLL_STEP_POS_MAX_U16        256u
#define MAX_CURRENT_REF_COUNTS      40   /* ~2 A with provisional scale */
#define MOD_LIMIT                 3277   /* 10% normalized bus modulation */
#define VOLTAGE_BENCH_MOD          700   /* ~2.1% bus */
#define OPENLOOP_STEP_START         32u
#define OPENLOOP_STEP_END          240u
#define PHASE_LOCK_LIMIT_U16       5461  /* 30 degrees */
#define DEFAULT_RUN_CURRENT_MA      500u
#define BEMF_VALID_MIN_MV           120u
#define BEMF_LPF_SHIFT                2u
#define CURRENT_LOOP_BW_RAD_S      1571u /* ~250 Hz */
#define FALLBACK_KP_MOD_PER_COUNT      6u
#define FALLBACK_KI_MOD_PER_COUNT      1u

static int32_t clamp32(int32_t x, int32_t lo, int32_t hi) { return x < lo ? lo : (x > hi ? hi : x); }
static uint32_t abs32u(int32_t x) { return (uint32_t)(x < 0 ? -x : x); }

static uint16_t fast_atan2_u16(int32_t y, int32_t x) {
    if ((x | y) == 0) return 0;
    uint32_t ax = abs32u(x), ay = abs32u(y), base;
    if (ax >= ay) {
        while (ax > 0x3FFFFu) { ax >>= 1; ay >>= 1; }
        uint32_t r = ax ? ((ay << 14) / ax) : 0u;
        base = (r * 8192u) >> 14;
    } else {
        while (ay > 0x3FFFFu) { ax >>= 1; ay >>= 1; }
        uint32_t r = ay ? ((ax << 14) / ay) : 0u;
        base = 16384u - ((r * 8192u) >> 14);
    }
    if (x >= 0 && y >= 0) return (uint16_t)base;
    if (x < 0 && y >= 0) return (uint16_t)(32768u - base);
    if (x < 0 && y < 0) return (uint16_t)(32768u + base);
    return (uint16_t)(65536u - base);
}

static int16_t sin_q15(uint16_t a) { return sin_lut_q15[a >> 8]; }
static int16_t cos_q15(uint16_t a) { return sin_lut_q15[(uint8_t)((a >> 8) + 64u)]; }
static int16_t phase_delta(uint16_t to, uint16_t from) { return (int16_t)(to - from); }

int32_t sensorless_counts_to_ma(int32_t counts) {
    return stock_current_nominal_counts_to_ma(counts);
}

static int16_t ma_to_counts(uint16_t ma) {
    int32_t counts = (int32_t)stock_current_nominal_ma_to_counts(ma);
    if (counts < 1) counts = 1;
    if (counts > MAX_CURRENT_REF_COUNTS) counts = MAX_CURRENT_REF_COUNTS;
    return (int16_t)counts;
}

static uint32_t vbus_mv_from_adc(uint16_t adc) {
    return (uint32_t)(((uint64_t)adc * VBUS_MV_PER_COUNT_Q8) >> 8);
}

static void zero_vector(sensorless_control_t *s, uint16_t pwm_arr) {
    uint16_t mid = (uint16_t)((pwm_arr + 1u) >> 1);
    s->ccr1 = mid; s->ccr2 = mid; s->ccr3 = mid;
    s->v_alpha_prev = 0; s->v_beta_prev = 0;
}

static void observer_reset(sensorless_control_t *s) {
    vesc_ebics_flux_reset(&s->vesc_flux);
    s->bemf_alpha_q8 = 0; s->bemf_beta_q8 = 0;
    s->i_alpha_prev_ma = 0; s->i_beta_prev_ma = 0;
    s->observer_phase = 0; s->observer_phase_prev = 0;
    s->observer_step = 0; s->bemf_mv = 0; s->observer_valid = 0;
    s->lock_ticks = 0; s->lost_ticks = 0;
}

#if !MOTOR_VOLTAGE_BENCH
static void update_pi_gains(sensorless_control_t *s, uint32_t vbus_mv) {
    uint32_t kp = FALLBACK_KP_MOD_PER_COUNT;
    uint32_t ki = FALLBACK_KI_MOD_PER_COUNT;
    if (s->motor_params_valid && vbus_mv >= 5000u) {
        /* 250 Hz current-loop target. Integer form of Kp=L*w and Ki=R*w,
         * normalized to Q15 bus modulation and the provisional current scale.
         * Use uH/mOhm/whole volts so Cortex-M3 needs no 64-bit runtime divide. */
        uint32_t l_uh = s->motor_l_nh / 1000u;
        uint32_t r_mohm = s->motor_r_uohm / 1000u;
        uint32_t vbus_v = vbus_mv / 1000u;
        if (vbus_v < 5u) vbus_v = 5u;
        kp = (l_uh * 2593u + (500u * vbus_v)) / (1000u * vbus_v);
        ki = (r_mohm * 648u + (500u * vbus_v)) / (1000u * vbus_v);
        if (kp < 1u) kp = 1u; if (kp > 200u) kp = 200u;
        if (ki < 1u) ki = 1u; if (ki > 100u) ki = 100u;
    }
    s->pi_kp_mod_per_count = (uint16_t)kp;
    s->pi_ki_mod_per_count = (uint16_t)ki;
}
#endif

static void observer_update(sensorless_control_t *s, int32_t i_alpha_counts,
                            int32_t i_beta_counts, uint16_t vbus_adc) {
    int32_t ia_ma = sensorless_counts_to_ma(i_alpha_counts);
    int32_t ib_ma = sensorless_counts_to_ma(i_beta_counts);
    uint32_t vbus_mv = vbus_mv_from_adc(vbus_adc);
    int32_t va_mv = (int32_t)(((int64_t)s->v_alpha_prev * (int64_t)vbus_mv) >> 15);
    int32_t vb_mv = (int32_t)(((int64_t)s->v_beta_prev * (int64_t)vbus_mv) >> 15);

    int32_t ra_mv = 0, rb_mv = 0, la_mv = 0, lb_mv = 0;
    if (s->motor_params_valid) {
        int32_t r_mohm = (int32_t)(s->motor_r_uohm / 1000u);
        int32_t l_uh = (int32_t)(s->motor_l_nh / 1000u);
        int32_t dia = clamp32(ia_ma - s->i_alpha_prev_ma, -10000, 10000);
        int32_t dib = clamp32(ib_ma - s->i_beta_prev_ma, -10000, 10000);
        ra_mv = (r_mohm * ia_ma) / 1000;
        rb_mv = (r_mohm * ib_ma) / 1000;
        la_mv = (l_uh * dia) / 250; /* uH*mA at 4 kHz -> mV */
        lb_mv = (l_uh * dib) / 250;
    }
    s->i_alpha_prev_ma = ia_ma; s->i_beta_prev_ma = ib_ma;

    int32_t ea_q8 = (int32_t)((int64_t)(va_mv - ra_mv - la_mv) * 256LL);
    int32_t eb_q8 = (int32_t)((int64_t)(vb_mv - rb_mv - lb_mv) * 256LL);
    s->bemf_alpha_q8 += (ea_q8 - s->bemf_alpha_q8) >> BEMF_LPF_SHIFT;
    s->bemf_beta_q8  += (eb_q8 - s->bemf_beta_q8) >> BEMF_LPF_SHIFT;

    uint32_t ae = abs32u(s->bemf_alpha_q8 >> 8);
    uint32_t be = abs32u(s->bemf_beta_q8 >> 8);
    uint32_t mx = ae > be ? ae : be, mn = ae > be ? be : ae;
    s->bemf_mv = mx + (mn >> 1); /* cheap magnitude estimate */

    uint16_t p;
#if VESC_EBICS_OBSERVER
    if (!s->motor_params_valid || !vesc_ebics_flux_step(&s->vesc_flux,
        va_mv, vb_mv, ia_ma, ib_ma, s->motor_r_uohm, s->motor_l_nh,
        s->motor_flux_uwb)) {
        s->observer_valid = 0u;
        return;
    }
    /* VESC Ortega residual x-Li is ROTOR FLUX, not back-EMF.
     * Do not apply the old BEMF -90-degree rotation here. */
    p = (uint16_t)(fast_atan2_u16(s->vesc_flux.flux_beta_nwb,
                                 s->vesc_flux.flux_alpha_nwb) + s->phase_offset_u16);
#else
    p = (uint16_t)(fast_atan2_u16(s->bemf_beta_q8, s->bemf_alpha_q8) - 16384u + s->phase_offset_u16);
#endif
    int16_t d = phase_delta(p, s->observer_phase_prev);
    s->observer_phase_prev = p;
    s->observer_step += (int16_t)((d - s->observer_step) >> 3);
    s->observer_phase = p;

    uint32_t max_reasonable = (vbus_mv * 3u) / 4u;
    /* PMSM sanity check: |E| ~= omega_e * flux. With E in mV and flux in
     * micro-weber, expected phase increment per 4 kHz tick is approximately
     * E_mV * 2608 / flux_uWb. Wide 4:1 bounds intentionally tolerate the
     * still-provisional voltage/current scaling while rejecting nonsense lock. */
    uint32_t expected_step = 0u;
    uint8_t speed_flux_ok = 0u;
    if (s->motor_params_valid && s->motor_flux_uwb != 0u) {
        expected_step = (s->bemf_mv * 2608u) / s->motor_flux_uwb;
        if (expected_step > 0u && s->observer_step > 0) {
            uint32_t obs_step = (uint16_t)s->observer_step;
            speed_flux_ok = (uint8_t)(obs_step * 4u >= expected_step &&
                                      obs_step <= expected_step * 4u);
        }
    }
    s->observer_valid = (uint8_t)(s->motor_params_valid &&
                                  s->bemf_mv >= BEMF_VALID_MIN_MV &&
                                  s->bemf_mv <= max_reasonable &&
                                  speed_flux_ok);
}

static uint32_t rolling_flux_magnitude_uwb(const sensorless_control_t *s) {
    uint32_t a=abs32u(s->vesc_flux.flux_alpha_nwb);
    uint32_t b=abs32u(s->vesc_flux.flux_beta_nwb);
    uint32_t mx=a>b?a:b,mn=a>b?b:a;
    return (mx+(mn>>1))/1000u;
}
static uint8_t rolling_phase_step_ok(int16_t step) {
    uint16_t a=(uint16_t)(step<0?-(int32_t)step:(int32_t)step);
    return (uint8_t)(a<=ROLL_STEP_ABS_MAX_U16 && step<=(int16_t)ROLL_STEP_POS_MAX_U16);
}
static uint8_t rolling_flux_instant_ok(const sensorless_control_t *s) {
    if(!s->motor_params_valid || !s->vesc_flux.valid || !s->motor_flux_uwb)return 0u;
    uint32_t mag=rolling_flux_magnitude_uwb(s);
    if(mag<s->motor_flux_uwb/2u || mag>((s->motor_flux_uwb*3u)/2u))return 0u;
    if(!rolling_phase_step_ok(s->observer_step))return 0u;
    return 1u;
}

static void vector_to_pwm(sensorless_control_t *s, int32_t v_alpha, int32_t v_beta, uint16_t pwm_arr) {
    s->v_alpha_prev = v_alpha; s->v_beta_prev = v_beta;
    const int32_t sqrt3_2_q15 = 28378;
    int32_t pa = v_alpha;
    int32_t pb = -(v_alpha >> 1) + (int32_t)(((int64_t)v_beta * sqrt3_2_q15) >> 15);
    int32_t pc = -(v_alpha >> 1) - (int32_t)(((int64_t)v_beta * sqrt3_2_q15) >> 15);
    int32_t vmax = pa, vmin = pa;
    if (pb > vmax) vmax = pb; if (pc > vmax) vmax = pc;
    if (pb < vmin) vmin = pb; if (pc < vmin) vmin = pc;
    int32_t center = -((vmax + vmin) >> 1);
    pa = clamp32(pa + center, -MOD_LIMIT, MOD_LIMIT);
    pb = clamp32(pb + center, -MOD_LIMIT, MOD_LIMIT);
    pc = clamp32(pc + center, -MOD_LIMIT, MOD_LIMIT);
    s->ccr1 = (uint16_t)(((uint32_t)(16384 + pa) * pwm_arr) >> 15);
    s->ccr2 = (uint16_t)(((uint32_t)(16384 + pb) * pwm_arr) >> 15);
    s->ccr3 = (uint16_t)(((uint32_t)(16384 + pc) * pwm_arr) >> 15);
}

/* A pure, motor-ID-specific frontend to the existing vector_to_pwm.
 * No changes to the normal drive calculation, identical phase ordering,
 * integer arithmetic throughout (Cortex-M3 has no FPU).
 */
uint8_t sensorless_id_voltage_to_pwm(int32_t amplitude_mv,uint16_t angle_u16,
                                     uint32_t vbus_mv,uint16_t arr,
                                     sensorless_id_pwm_t *out) {
    if (!out || amplitude_mv<=0 || amplitude_mv>2000 ||
        vbus_mv<5000u || vbus_mv>65000u || !arr || arr>65535u)
        return 0u;
    /* Prevent silently clipping the requested voltage. Voltage injection
     * is not equivalent to a current-limited supply. */
    const uint32_t normalized=((uint32_t)amplitude_mv*32768u+vbus_mv/2u)/vbus_mv;
    if (!normalized || normalized>MOD_LIMIT) return 0u;
    int32_t alpha=(int32_t)(((int32_t)normalized*(int32_t)cos_q15(angle_u16))/32768);
    int32_t beta =(int32_t)(((int32_t)normalized*(int32_t)sin_q15(angle_u16))/32768);
    sensorless_control_t temp={0};
    vector_to_pwm(&temp,alpha,beta,arr);
    out->ccr1=temp.ccr1;out->ccr2=temp.ccr2;out->ccr3=temp.ccr3;
    out->alpha_q15=alpha;out->beta_q15=beta;
    return 1u;
}

#if MOTOR_VOLTAGE_BENCH
static void voltage_openloop(sensorless_control_t *s, uint16_t phase, uint16_t pwm_arr) {
    int32_t sn = sin_q15(phase), cs = cos_q15(phase);
    /* q-axis voltage only: alpha=-Vq*sin, beta=Vq*cos. */
    int32_t va = (int32_t)(-((int64_t)VOLTAGE_BENCH_MOD * sn) >> 15);
    int32_t vb = (int32_t)(((int64_t)VOLTAGE_BENCH_MOD * cs) >> 15);
    vector_to_pwm(s, va, vb, pwm_arr);
}
#endif

#if !MOTOR_VOLTAGE_BENCH
static void limit_dq(int32_t *vd, int32_t *vq) {
    uint32_t ad = abs32u(*vd), aq = abs32u(*vq);
    uint32_t mx = ad > aq ? ad : aq, mn = ad > aq ? aq : ad;
    uint32_t mag = mx + (mn >> 1);
    if (mag > MOD_LIMIT && mag != 0u) {
        *vd = (*vd * MOD_LIMIT) / (int32_t)mag;
        *vq = (*vq * MOD_LIMIT) / (int32_t)mag;
    }
}

static void current_control(sensorless_control_t *s, int32_t i_alpha, int32_t i_beta,
                            uint16_t phase, int32_t id_ref, int32_t iq_ref,
                            uint16_t vbus_adc, uint16_t pwm_arr) {
    int32_t sn = sin_q15(phase), cs = cos_q15(phase);
    s->id = (int32_t)(((int64_t)i_alpha * cs + (int64_t)i_beta * sn) >> 15);
    s->iq = (int32_t)((-(int64_t)i_alpha * sn + (int64_t)i_beta * cs) >> 15);
    id_ref = clamp32(id_ref, -MAX_CURRENT_REF_COUNTS, MAX_CURRENT_REF_COUNTS);
    iq_ref = clamp32(iq_ref, -MAX_CURRENT_REF_COUNTS, MAX_CURRENT_REF_COUNTS);
    int32_t ed = id_ref - s->id, eq = iq_ref - s->iq;

    update_pi_gains(s, vbus_mv_from_adc(vbus_adc));
    s->id_int = clamp32(s->id_int + ed * s->pi_ki_mod_per_count, -MOD_LIMIT, MOD_LIMIT);
    s->iq_int = clamp32(s->iq_int + eq * s->pi_ki_mod_per_count, -MOD_LIMIT, MOD_LIMIT);
    int32_t vd = s->id_int + ed * s->pi_kp_mod_per_count;
    int32_t vq = s->iq_int + eq * s->pi_kp_mod_per_count;
    limit_dq(&vd, &vq);
    s->id_int = clamp32(s->id_int, -MOD_LIMIT, MOD_LIMIT);
    s->iq_int = clamp32(s->iq_int, -MOD_LIMIT, MOD_LIMIT);

    int32_t v_alpha = (int32_t)(((int64_t)vd * cs - (int64_t)vq * sn) >> 15);
    int32_t v_beta  = (int32_t)(((int64_t)vd * sn + (int64_t)vq * cs) >> 15);
    vector_to_pwm(s, v_alpha, v_beta, pwm_arr);
}
#endif

void sensorless_control_init(sensorless_control_t *s) {
    s->drive_request = 0; s->state = SENSORLESS_STOP; s->fault = 0; s->state_ticks = 0;
    s->openloop_step = OPENLOOP_STEP_START; s->openloop_phase = 0; s->control_phase = 0;
    s->rolling_step=0;s->rolling_good_ticks=0u;s->rolling_iq_ticks=0u;
    s->rolling_clamp_start=0u;s->rolling_phase_error_sum=0u;s->rolling_start=0u;
    s->id = 0; s->iq = 0; s->id_int = 0; s->iq_int = 0;
    s->motor_r_uohm = 0; s->motor_l_nh = 0; s->motor_flux_uwb = 0; s->motor_params_valid = 0;
    s->phase_offset_u16 = 0u; s->run_current_ma = DEFAULT_RUN_CURRENT_MA; s->run_iq_counts = ma_to_counts(DEFAULT_RUN_CURRENT_MA);
    s->current_sum_counts = 0; s->pi_kp_mod_per_count = FALLBACK_KP_MOD_PER_COUNT; s->pi_ki_mod_per_count = FALLBACK_KI_MOD_PER_COUNT;
    observer_reset(s); s->pwm_arr_last = 1999u; zero_vector(s,s->pwm_arr_last);
}

void sensorless_control_set_phase_offset(sensorless_control_t *s, int16_t offset) { s->phase_offset_u16 = (uint16_t)offset; }

uint8_t sensorless_control_set_run_current_ma(sensorless_control_t *s, uint16_t current_ma) {
    if (current_ma < 100u || current_ma > 2000u) return 0u;
    s->run_current_ma = current_ma; s->run_iq_counts = ma_to_counts(current_ma); return 1u;
}

uint8_t sensorless_control_set_motor_params(sensorless_control_t *s, uint32_t r_uohm,
                                            uint32_t l_nh, uint32_t flux_uwb) {
    if (r_uohm < 1000u || r_uohm > 2000000u) return 0u;
    if (l_nh < 1000u || l_nh > 5000000u) return 0u;
    if (flux_uwb < 100u || flux_uwb > 1000000u) return 0u;
    s->motor_r_uohm = r_uohm; s->motor_l_nh = l_nh; s->motor_flux_uwb = flux_uwb;
    s->motor_params_valid = 1u; observer_reset(s); return 1u;
}

uint8_t sensorless_control_start_rolling(sensorless_control_t *s,
                                         uint16_t phase_u16,
                                         int16_t electrical_step_u16) {
    if(!s || s->fault || s->state==SENSORLESS_FAULT || !s->motor_params_valid ||
       !s->motor_flux_uwb || electrical_step_u16>=0 ||
       !rolling_phase_step_ok(electrical_step_u16)) return 0u;
    s->drive_request=1u;s->state=SENSORLESS_ROLL_VERIFY;s->state_ticks=0u;
    s->openloop_phase=phase_u16;s->control_phase=phase_u16;
    s->rolling_step=electrical_step_u16;s->rolling_good_ticks=0u;
    s->rolling_iq_ticks=0u;s->rolling_phase_error_sum=0u;s->rolling_start=1u;
    s->id_int=0;s->iq_int=0;observer_reset(s);
    const int32_t lambda=(int32_t)(s->motor_flux_uwb*1000u);
    /* phase_u16 is already in FOC control frame. The observer's internal
     * x/flux vector is physical alpha/beta, therefore remove the learned
     * Flux->FOC frame offset before seeding. Observer_update() adds it back
     * after atan2. This avoids applying the ~117 deg calibration twice. */
    const uint16_t seed_flux_phase=(uint16_t)(phase_u16-s->phase_offset_u16);
    const int32_t fa=(int32_t)(((int64_t)lambda*cos_q15(seed_flux_phase))>>15);
    const int32_t fb=(int32_t)(((int64_t)lambda*sin_q15(seed_flux_phase))>>15);
    s->vesc_flux.x_alpha_nwb=fa;s->vesc_flux.x_beta_nwb=fb;
    s->vesc_flux.flux_alpha_nwb=fa;s->vesc_flux.flux_beta_nwb=fb;
    s->vesc_flux.samples=2u;s->vesc_flux.valid=1u;
    s->observer_phase=s->observer_phase_prev=phase_u16;
    s->observer_step=electrical_step_u16;
    s->rolling_clamp_start=s->vesc_flux.clamp_events;
    return 1u;
}

void sensorless_control_set_drive(sensorless_control_t *s, uint8_t enable) {
    if (!enable) { sensorless_control_stop(s); return; }
    /* A latched observer fault cannot be cleared by another drive request. */
    if (s->fault || s->state == SENSORLESS_FAULT) return;
    if (!s->drive_request) {
        s->rolling_start=0u;
        s->drive_request = 1; s->state = SENSORLESS_ALIGN; s->state_ticks = 0;
        s->openloop_step = OPENLOOP_STEP_START; s->openloop_phase = 0; s->control_phase = 0;
        s->id_int = 0; s->iq_int = 0; observer_reset(s);
    }
}

void sensorless_control_stop(sensorless_control_t *s) {
    s->drive_request = 0; s->state = SENSORLESS_STOP; s->state_ticks = 0;
    s->lock_ticks = 0; s->lost_ticks = 0; s->id_int = 0; s->iq_int = 0;
    s->rolling_good_ticks=0u;s->rolling_iq_ticks=0u;s->rolling_phase_error_sum=0u;
    s->rolling_start=0u;s->rolling_step=0;
    /* Zero CCR values would mean 0% duty, not neutral voltage.  Always
       neutralize to common-mode 50% immediately, without another IRQ.
       Physical coast still requires the separate, hardware-verified gate kill. */
    zero_vector(s,s->pwm_arr_last);
}

void sensorless_control_step(sensorless_control_t *s, phase_current_counts_t current,
                             uint16_t vbus_adc, uint16_t pwm_arr, uint8_t power_armed) {
    if (!pwm_arr) { sensorless_control_stop(s); return; }
    s->pwm_arr_last = pwm_arr;
    /* Remove three-shunt common-mode error before Clarke transform. */
    int32_t sum = (int32_t)current.ia + current.ib + current.ic;
    s->current_sum_counts = (int16_t)sum;
    int32_t cm = sum / 3;
    int32_t ia = (int32_t)current.ia - cm;
    int32_t ib = (int32_t)current.ib - cm;
    int32_t ic = (int32_t)current.ic - cm;
    int32_t i_alpha = ia;
    int32_t i_beta = (int32_t)(((int64_t)(ib - ic) * 18919) >> 15); /* (Ib-Ic)/sqrt(3) */

    /* Do not run the BEMF estimator on unpowered or invalidated inputs. */
    if (!power_armed || !s->drive_request || s->fault) {
        if (s->fault) s->state = SENSORLESS_FAULT;
        else s->state = SENSORLESS_STOP;
        s->state_ticks = 0; s->id_int = 0; s->iq_int = 0;
        zero_vector(s, pwm_arr); return;
    }
    observer_update(s, i_alpha, i_beta, vbus_adc);
    s->phase_error = phase_delta(s->observer_phase, s->openloop_phase);

    s->state_ticks++;
    switch ((sensorless_state_id_t)s->state) {
    case SENSORLESS_STOP:
        s->state = SENSORLESS_ALIGN; s->state_ticks = 0; s->openloop_phase = 0; s->control_phase = 0; break;
    case SENSORLESS_ALIGN:
        s->control_phase = 0u;
        if (s->state_ticks >= ALIGN_TICKS) { s->state = SENSORLESS_OPEN_LOOP; s->state_ticks = 0; s->lock_ticks = 0; }
        break;
    case SENSORLESS_OPEN_LOOP: {
        uint32_t ramp = s->state_ticks; if (ramp > CONTROL_HZ) ramp = CONTROL_HZ;
        s->openloop_step = (uint16_t)(OPENLOOP_STEP_START + ((OPENLOOP_STEP_END - OPENLOOP_STEP_START) * ramp) / CONTROL_HZ);
        s->openloop_phase = (uint16_t)(s->openloop_phase + s->openloop_step); s->control_phase = s->openloop_phase;
#if SENSORLESS_CLOSED_LOOP_ALLOWED
        int32_t pe = s->phase_error; if (pe < 0) pe = -pe;
        if (s->motor_params_valid && s->observer_valid && pe < PHASE_LOCK_LIMIT_U16 && s->state_ticks > OPEN_LOOP_MIN_TICKS) {
            if (s->lock_ticks < 0xFFFFu) s->lock_ticks++;
        } else s->lock_ticks = 0;
        if (s->lock_ticks >= LOCK_REQUIRED_TICKS) { s->state = SENSORLESS_HANDOVER; s->state_ticks = 0; }
        else if (s->state_ticks >= OPEN_LOOP_TIMEOUT_TICKS) { s->fault = 1; s->state = SENSORLESS_FAULT; }
#else
        if (s->state_ticks >= OPEN_LOOP_BENCH_TICKS) { sensorless_control_stop(s); zero_vector(s,pwm_arr); return; }
#endif
        break;
    }
    case SENSORLESS_HANDOVER: {
#if SENSORLESS_CLOSED_LOOP_ALLOWED
        s->openloop_phase = (uint16_t)(s->openloop_phase + s->openloop_step);
        int16_t d = phase_delta(s->observer_phase, s->openloop_phase);
        uint32_t blend = s->state_ticks; if (blend > HANDOVER_TICKS) blend = HANDOVER_TICKS;
        s->control_phase = (uint16_t)(s->openloop_phase + ((int32_t)d * (int32_t)blend) / (int32_t)HANDOVER_TICKS);
        if (!s->observer_valid) { if (s->lost_ticks < 0xFFFFu) s->lost_ticks++; } else s->lost_ticks = 0;
        if (s->lost_ticks >= OBS_LOST_TICKS) { s->fault = 2; s->state = SENSORLESS_FAULT; }
        else if (s->state_ticks >= HANDOVER_TICKS) { s->state = SENSORLESS_CLOSED_LOOP; s->state_ticks = 0; s->lost_ticks = 0; }
#else
        s->state = SENSORLESS_FAULT; s->fault = 4u;
#endif
        break;
    }
    case SENSORLESS_CLOSED_LOOP:
#if SENSORLESS_CLOSED_LOOP_ALLOWED
        s->control_phase = s->observer_phase;
        const uint8_t obs_ok=s->rolling_start?rolling_flux_instant_ok(s):s->observer_valid;
        if (!obs_ok) { if (s->lost_ticks < 0xFFFFu) s->lost_ticks++; } else s->lost_ticks = 0;
        if (s->lost_ticks >= OBS_LOST_TICKS) { s->fault = 3; s->state = SENSORLESS_FAULT; }
#else
        s->state = SENSORLESS_FAULT; s->fault = 5u;
#endif
        break;
    case SENSORLESS_ROLL_VERIFY: {
#if SENSORLESS_CLOSED_LOOP_ALLOWED
        s->openloop_phase=(uint16_t)(s->openloop_phase+s->rolling_step);
        s->control_phase=s->openloop_phase;
        const int16_t pe_s=phase_delta(s->observer_phase,s->openloop_phase);
        const uint16_t pe=(uint16_t)(pe_s<0?-(int32_t)pe_s:(int32_t)pe_s);
        const uint16_t clamp_delta=(uint16_t)(s->vesc_flux.clamp_events-s->rolling_clamp_start);
        if(pe>=ROLL_PHASE_OVER90_U16 || clamp_delta || !rolling_phase_step_ok(s->observer_step)){
            s->fault=6u;s->state=SENSORLESS_FAULT;break;
        }
        if(rolling_flux_instant_ok(s) && pe<ROLL_PHASE_NOW_MAX_U16){
            if(s->rolling_good_ticks<65535u)s->rolling_good_ticks++;
            s->rolling_phase_error_sum+=pe;
        }else{
            s->rolling_good_ticks=0u;s->rolling_phase_error_sum=0u;
        }
        if(s->rolling_good_ticks>=ROLL_VERIFY_TICKS){
            const uint32_t avg=s->rolling_phase_error_sum/s->rolling_good_ticks;
            if(avg>=ROLL_PHASE_AVG_MAX_U16){s->fault=7u;s->state=SENSORLESS_FAULT;break;}
            s->state=SENSORLESS_CLOSED_LOOP;s->state_ticks=0u;s->lost_ticks=0u;
            s->rolling_iq_ticks=0u;
        }else if(s->state_ticks>=ROLL_VERIFY_TIMEOUT_TICKS){
            s->fault=8u;s->state=SENSORLESS_FAULT;
        }
#else
        s->fault=9u;s->state=SENSORLESS_FAULT;
#endif
        break;
    }
    case SENSORLESS_FAULT:
    default:
        s->drive_request = 0; zero_vector(s, pwm_arr); return;
    }
    /* The state machine may LATCH A FAULT inside this switch (observer
       timeout/loss).  Do not emit one more energized PWM calculation. */
    if (s->state == SENSORLESS_FAULT || s->fault) {
        s->drive_request = 0u; s->id_int = 0; s->iq_int = 0;
        zero_vector(s, pwm_arr);
        return;
    }

#if MOTOR_VOLTAGE_BENCH
    if (s->state == SENSORLESS_ALIGN) {
        /* Stationary d-axis vector at very low modulation. */
        vector_to_pwm(s, VOLTAGE_BENCH_MOD, 0, pwm_arr);
    } else {
        voltage_openloop(s, s->control_phase, pwm_arr);
    }
#else
    int32_t id_ref = (s->state == SENSORLESS_ALIGN) ? s->run_iq_counts : 0;
    int32_t iq_ref = (s->state == SENSORLESS_ALIGN) ? 0 : s->run_iq_counts;
    if(s->rolling_start){
        id_ref=0;
        if(s->state==SENSORLESS_ROLL_VERIFY)iq_ref=0;
        else if(s->state==SENSORLESS_CLOSED_LOOP){
            int32_t target=s->run_iq_counts;
            if(target>ROLL_MAX_IQ_COUNTS)target=ROLL_MAX_IQ_COUNTS;
            if(s->rolling_iq_ticks<ROLL_IQ_RAMP_TICKS)s->rolling_iq_ticks++;
            iq_ref=(int32_t)(((int64_t)target*s->rolling_iq_ticks)/ROLL_IQ_RAMP_TICKS);
        }else iq_ref=0;
    }
    current_control(s, i_alpha, i_beta, s->control_phase, id_ref, iq_ref, vbus_adc, pwm_arr);
#endif
}

const char *sensorless_state_name(uint8_t state) {
    switch ((sensorless_state_id_t)state) {
    case SENSORLESS_STOP: return "STOP"; case SENSORLESS_ALIGN: return "ALIGN"; case SENSORLESS_OPEN_LOOP: return "OPEN";
    case SENSORLESS_HANDOVER: return "HANDOVER"; case SENSORLESS_CLOSED_LOOP: return "CLOSED"; case SENSORLESS_FAULT: return "FAULT"; case SENSORLESS_ROLL_VERIFY: return "ROLL_VERIFY";
    default: return "?";
    }
}
