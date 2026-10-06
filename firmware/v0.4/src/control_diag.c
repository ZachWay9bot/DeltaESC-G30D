#include "control_diag.h"

static const int16_t sin_lut_q15[256] = {
#include "sin_q15.inc"
};

static inline int32_t clamp32(int32_t x, int32_t lo, int32_t hi) {
    return x < lo ? lo : (x > hi ? hi : x);
}

static uint16_t fast_atan2_u16(int32_t y, int32_t x) {
    // Integer octant approximation. Angle: 0..65535 == 0..2*pi.
    if ((x | y) == 0) return 0;
    uint32_t ax = (uint32_t)(x < 0 ? -x : x);
    uint32_t ay = (uint32_t)(y < 0 ? -y : y);
    uint32_t base;
    if (ax >= ay) {
        uint32_t r = ax ? ((ay << 14) / ax) : 0;
        base = (r * 8192u) >> 14;            // 0..45 deg
    } else {
        uint32_t r = ay ? ((ax << 14) / ay) : 0;
        base = 16384u - ((r * 8192u) >> 14); // 45..90 deg
    }
    if (x >= 0 && y >= 0) return (uint16_t)base;
    if (x < 0 && y >= 0)  return (uint16_t)(32768u - base);
    if (x < 0 && y < 0)   return (uint16_t)(32768u + base);
    return (uint16_t)(65536u - base);
}

static inline int16_t sin_q15(uint16_t a) { return sin_lut_q15[a >> 8]; }
static inline int16_t cos_q15(uint16_t a) { return sin_lut_q15[(uint8_t)((a >> 8) + 64u)]; }

void control_diag_init(control_diag_state_t *s) {
    s->x_alpha = 1 << 18;
    s->x_beta = 0;
    s->i_alpha_prev = 0;
    s->i_beta_prev = 0;
    s->phase = 0;
    s->id_int = 0;
    s->iq_int = 0;
    s->v_alpha_prev = 0;
    s->v_beta_prev = 0;
    s->ccr1 = s->ccr2 = s->ccr3 = 0;
}

void control_diag_step(control_diag_state_t *s, phase_current_counts_t i, uint16_t pwm_arr) {
    // Clarke in ADC-count domain. 18919 = (1/sqrt(3))*32768.
    int32_t i_alpha = i.ia;
    int32_t i_beta = ((i.ia + 2 * (int32_t)i.ib) * 18919) >> 15;

    // Lightweight fixed-point flux observer using the same integration structure
    // as the well-known MXLEMMING/VESC family: integrate (v - R*i) and subtract
    // L*di. Constants are deliberately unitless placeholders in v0.3; only the
    // control-path timing is validated here. Real R/L/flux comes later.
    const int32_t R_Q15 = 900;      // placeholder, timing only
    const int32_t DT_Q15 = 8192;    // scaled integration gain
    const int32_t L_Q15 = 1200;     // placeholder, timing only
    int32_t ria = (i_alpha * R_Q15) >> 15;
    int32_t rib = (i_beta  * R_Q15) >> 15;
    int32_t dia = i_alpha - s->i_alpha_prev;
    int32_t dib = i_beta  - s->i_beta_prev;
    s->x_alpha += ((s->v_alpha_prev - ria) * DT_Q15) >> 15;
    s->x_beta  += ((s->v_beta_prev  - rib) * DT_Q15) >> 15;
    s->x_alpha -= (dia * L_Q15) >> 15;
    s->x_beta  -= (dib * L_Q15) >> 15;
    s->i_alpha_prev = i_alpha;
    s->i_beta_prev = i_beta;

    const int32_t FLUX_LIM = 1 << 20;
    s->x_alpha = clamp32(s->x_alpha, -FLUX_LIM, FLUX_LIM);
    s->x_beta  = clamp32(s->x_beta,  -FLUX_LIM, FLUX_LIM);
    s->phase = fast_atan2_u16(s->x_beta, s->x_alpha);

    int32_t sn = sin_q15(s->phase);
    int32_t cs = cos_q15(s->phase);
    int32_t id = (i_alpha * cs + i_beta * sn) >> 15;
    int32_t iq = (-i_alpha * sn + i_beta * cs) >> 15;

    // Passive diagnostic: zero current references. The complete PI/Park/SVPWM
    // path still executes, but no physical PWM output can become active.
    int32_t ed = -id;
    int32_t eq = -iq;
    const int32_t KP_Q15 = 6000;
    const int32_t KI_Q15 = 550;
    s->id_int = clamp32(s->id_int + ((ed * KI_Q15) >> 15), -12000, 12000);
    s->iq_int = clamp32(s->iq_int + ((eq * KI_Q15) >> 15), -12000, 12000);
    int32_t vd = clamp32(((ed * KP_Q15) >> 15) + s->id_int, -14000, 14000);
    int32_t vq = clamp32(((eq * KP_Q15) >> 15) + s->iq_int, -14000, 14000);

    int32_t va = (vd * cs - vq * sn) >> 15;
    int32_t vb = (vd * sn + vq * cs) >> 15;
    s->v_alpha_prev = va;
    s->v_beta_prev = vb;

    // Inverse Clarke followed by zero-sequence centering (SVPWM-like duty set).
    const int32_t SQRT3_2_Q15 = 28378;
    int32_t phase_a = va;
    int32_t phase_b = -(va >> 1) + ((vb * SQRT3_2_Q15) >> 15);
    int32_t phase_c = -(va >> 1) - ((vb * SQRT3_2_Q15) >> 15);
    int32_t vmax = phase_a;
    int32_t vmin = phase_a;
    if (phase_b > vmax) vmax = phase_b; if (phase_c > vmax) vmax = phase_c;
    if (phase_b < vmin) vmin = phase_b; if (phase_c < vmin) vmin = phase_c;
    int32_t center = -((vmax + vmin) >> 1);
    phase_a = clamp32(phase_a + center, -15000, 15000);
    phase_b = clamp32(phase_b + center, -15000, 15000);
    phase_c = clamp32(phase_c + center, -15000, 15000);

    int32_t da = clamp32(16384 + phase_a, 512, 32256);
    int32_t db = clamp32(16384 + phase_b, 512, 32256);
    int32_t dc = clamp32(16384 + phase_c, 512, 32256);
    s->ccr1 = (uint16_t)(((uint32_t)da * pwm_arr) >> 15);
    s->ccr2 = (uint16_t)(((uint32_t)db * pwm_arr) >> 15);
    s->ccr3 = (uint16_t)(((uint32_t)dc * pwm_arr) >> 15);
}
