/* DeltaESC G30D - TIM1 BDTR software contract, STM32F103 RM0008.
 * Diagnostic-only invariant. NOT proof of driver HIN/LIN or AMP_LIM polarity.
 * GPL-3.0-or-later.
 */
#ifndef TIM1_BDTR_GUARD_H
#define TIM1_BDTR_GUARD_H
#include <stdint.h>

#define D_GUARD_MOE   0x8000u
#define D_GUARD_AOE   0x4000u
#define D_GUARD_BKP   0x2000u
#define D_GUARD_BKE   0x1000u
#define D_GUARD_OSSR  0x0800u
#define D_GUARD_OSSI  0x0400u
#define D_GUARD_LOCK  0x0300u
#define D_GUARD_CH4   0x1000u
#define D_GUARD_SIX   0x0555u

/* Do not rewrite BDTR when a bootloader or earlier code has already locked
 * polarity/break settings. MCU reset, not a register write, is required.
 * With unknown BKIN electrical polarity, BKE must NOT be guessed. */
static inline uint8_t tim1_bdtr_guard_preinit(uint16_t prior) {
    return (uint8_t)((prior & (D_GUARD_AOE | D_GUARD_BKP |
                              D_GUARD_BKE | D_GUARD_LOCK)) == 0u);
}

/* MOE is preserved in the passive ADC CC4-only mode, to avoid silently
 * invalidating TIM1-CC4 injected ADC triggers on the STM32F1. MOE alone is
 * NOT permission to drive MOSFETs: all CH1-3 and N outputs remain disabled.
 * Do not treat this software-only state as a qualified hardware coast. */
static inline uint8_t tim1_bdtr_guard_passive(uint16_t bdtr,
                                               uint8_t expected_dtg,
                                               uint16_t ccer) {
    const uint16_t expected=(uint16_t)(D_GUARD_MOE | D_GUARD_OSSR |
                              D_GUARD_OSSI | (uint16_t)expected_dtg);
    return (uint8_t)(bdtr==expected && ccer==D_GUARD_CH4);
}

/* Detect any gate channel unexpectedly enabled, including a polarity-bit
 * change that the previous six-bit mask alone would not catch. */
static inline uint8_t tim1_bdtr_guard_six_off(uint16_t ccer) {
    return (uint8_t)((ccer & D_GUARD_SIX)==0u);
}
#endif
