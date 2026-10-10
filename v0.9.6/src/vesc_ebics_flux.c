/*
 * VESC Ortega observer equation with EBiCS-style integer/F103 adaptation.
 * GPL-3.0-or-later. See vesc_ebics_flux.h for attribution.
 * x_dot = v - R*i + gamma/2 * (x - L*i) * (lambda^2 - |x-L*i|^2)
 * VESC clamps positive flux-radius error to zero. We normalize that error
 * to Q16 before applying a deliberately bounded discrete observer gain.
 * Dynamics are not assumed validated on G30D hardware.
 */
#include "vesc_ebics_flux.h"

static int32_t bounded(int64_t v, int32_t limit) {
    if (v > limit) return limit;
    if (v < -(int64_t)limit) return -limit;
    return (int32_t)v;
}
void vesc_ebics_flux_reset(vesc_ebics_flux_t *s) {
    if (!s) return;
    *s = (vesc_ebics_flux_t){0};
}
uint8_t vesc_ebics_flux_step(vesc_ebics_flux_t *s,
        int32_t va_mv, int32_t vb_mv, int32_t ia_ma, int32_t ib_ma,
        uint32_t r_uohm, uint32_t l_nh, uint32_t flux_uwb) {
    if (!s || r_uohm < 1000u || r_uohm > 2000000u ||
        l_nh < 1000u || l_nh > 5000000u ||
        flux_uwb < 100u || flux_uwb > 1000000u ||
        va_mv < -200000 || va_mv > 200000 ||
        vb_mv < -200000 || vb_mv > 200000 ||
        ia_ma < -20000 || ia_ma > 20000 ||
        ib_ma < -20000 || ib_ma > 20000) {
        if (s) s->valid=0u;
        return 0u;
    }
    const int32_t lambda = (int32_t)(flux_uwb * 1000u); /* uWb -> nWb */
    const int32_t limit = lambda <= 1000000000 ? lambda * 2 : 2000000000;
    const int32_t ri_a_mv = ((int32_t)(r_uohm / 1000u) * ia_ma) / 1000;
    const int32_t ri_b_mv = ((int32_t)(r_uohm / 1000u) * ib_ma) / 1000;
    const int32_t li_a_nwb = ((int32_t)(l_nh / 100u) * ia_ma) / 10;
    const int32_t li_b_nwb = ((int32_t)(l_nh / 100u) * ib_ma) / 10;
    const int32_t ea = bounded((int64_t)s->x_alpha_nwb - li_a_nwb,limit);
    const int32_t eb = bounded((int64_t)s->x_beta_nwb - li_b_nwb,limit);
    const int32_t unit = (lambda >> 8) ? (lambda >> 8) : 1;
    const int32_t eqa = ea / unit, eqb = eb / unit;
    int32_t err_q16 = 65536 - (eqa * eqa + eqb * eqb);
    if (err_q16 > 0) err_q16 = 0; /* VESC Ortega: no positive feedback */
    if (err_q16 < -131072) err_q16 = -131072;
    /* gamma is dimensionless AFTER normalizing the flux-radius error.
     * 512/32768 = 0.015625 per tick (bounded by error).
     * Uses only multiplication, shift and 32-bit divide on Cortex-M3. */
    const int32_t correction_a = (int32_t)(((int64_t)ea * err_q16 * 512) >> 31);
    const int32_t correction_b = (int32_t)(((int64_t)eb * err_q16 * 512) >> 31);
    const int32_t delta_a = (va_mv - ri_a_mv) * 250; /* mV*dt= nWb at 4kHz */
    const int32_t delta_b = (vb_mv - ri_b_mv) * 250;
    s->x_alpha_nwb = bounded((int64_t)s->x_alpha_nwb + delta_a + correction_a, limit);
    s->x_beta_nwb = bounded((int64_t)s->x_beta_nwb + delta_b + correction_b, limit);
    s->flux_alpha_nwb = bounded((int64_t)s->x_alpha_nwb - li_a_nwb,limit);
    s->flux_beta_nwb  = bounded((int64_t)s->x_beta_nwb  - li_b_nwb,limit);
    if (s->samples != 65535u) s->samples++;
    s->valid = (uint8_t)(s->samples >= 2u);
    return 1u;
}