/* DeltaESC v0.9.17: EBiCS-inspired latched MOE kill.
 * Pure 16-bit register contract. Hardware behavior NOT validated.
 * GPL-3.0-or-later.
 */
#ifndef TIM1_EMERGENCY_STOP_H
#define TIM1_EMERGENCY_STOP_H
#include <stdint.h>
#include "tim1_bdtr_guard.h"

/* Invariant after an emergency shutdown: MOE=0, all 6 enable bits off,
 * AOE=0 (no timer-update auto-reenable), no unexpected break/lock settings.
 * CH4E can remain enabled, but CH4 ADC triggering is NOT guaranteed with
 * MOE=0 on STM32F103. It is deliberately forfeited on a latched fault. */
static inline uint8_t tim1_emergency_registers_off(uint16_t bdtr,
                                                  uint16_t ccer) {
    return (uint8_t)(((bdtr & (D_GUARD_MOE | D_GUARD_AOE |
                              D_GUARD_BKE | D_GUARD_BKP |
                              D_GUARD_LOCK)) == 0u) &&
                     ((ccer & D_GUARD_SIX) == 0u));
}

typedef struct {
    uint16_t bdtr;
    uint16_t ccer;
    uint8_t killed;
} tim1_emergency_model_t;

/* Host register model for idempotency, E6, brake, watchdog, ADC fault and
 * inadvertent auto-reenable; not an electrical model of MT8006A. */
static inline void tim1_emergency_model_stop(tim1_emergency_model_t *m) {
    m->killed=1u;
    m->ccer=(uint16_t)(m->ccer & (uint16_t)~D_GUARD_SIX);
    m->bdtr=(uint16_t)(m->bdtr & (uint16_t)~D_GUARD_MOE);
}
static inline uint8_t tim1_emergency_model_can_arm(
    const tim1_emergency_model_t *m) {
    return (uint8_t)(!m->killed &&
       ((m->bdtr & (D_GUARD_AOE|D_GUARD_BKE|
                    D_GUARD_BKP|D_GUARD_LOCK))==0u));
}
#endif
