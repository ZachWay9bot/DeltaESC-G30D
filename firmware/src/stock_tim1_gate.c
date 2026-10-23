#include "stock_tim1_gate.h"
/* Disable ONLY six bridge output enables. Keep CH4 enabled so that
 * passive ADC diagnostics can continue with gate drive disconnected. */
uint16_t stock_tim1_gate_ccer_off(uint16_t previous) {
    return (uint16_t)(previous & (uint16_t)~STOCK_TIM1_GATE_CH_MASK);
}
/* DRV126 writes 0x1555, not "previous | CCxE": stale polarity bits
 * in CCER would otherwise survive and make one phase inverted. */
uint16_t stock_tim1_gate_ccer_arm(uint16_t previous) {
    (void)previous;
    return STOCK_TIM1_GATE_RUN_CCER;
}
uint8_t stock_tim1_gate_ccer_is_armed(uint16_t ccer) {
    return (uint8_t)(ccer == STOCK_TIM1_GATE_RUN_CCER);
}
uint8_t stock_tim1_gate_ccer_is_off(uint16_t ccer) {
    return (uint8_t)((ccer & STOCK_TIM1_GATE_CH_MASK) == 0u);
}
