#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "stock_tim1_gate.h"
int main(void) {
    /* Exact DRV126 CCER including three complementary gate pairs. */
    assert(STOCK_TIM1_GATE_RUN_CCER == 0x1555u);
    assert(STOCK_TIM1_GATE_CH_MASK == 0x0555u);
    assert(STOCK_TIM1_GATE_ADC_CC4E == 0x1000u);
    assert(stock_tim1_gate_ccer_arm(0x1000u) == 0x1555u);
    assert(stock_tim1_gate_ccer_arm(0x3555u) == 0x1555u);
    assert(stock_tim1_gate_ccer_arm(0xFFFFu) == 0x1555u);
    assert(stock_tim1_gate_ccer_is_armed(0x1555u));
    assert(!stock_tim1_gate_ccer_is_armed(0x3555u));
    assert(!stock_tim1_gate_ccer_is_armed(0x1000u));
    assert(stock_tim1_gate_ccer_off(0x1555u) == 0x1000u);
    assert(stock_tim1_gate_ccer_off(0x3555u) == 0x3000u);
    assert(stock_tim1_gate_ccer_is_off(0x1000u));
    assert(!stock_tim1_gate_ccer_is_off(0x1555u));
    /* OFF must be idempotent and must never re-arm any gate. */
    for (uint32_t v = 0; v < 65536u; v++) {
        uint16_t off=stock_tim1_gate_ccer_off((uint16_t)v);
        assert((off & 0x0555u) == 0u);
        assert(stock_tim1_gate_ccer_off(off) == off);
        assert(stock_tim1_gate_ccer_arm((uint16_t)v) == 0x1555u);
    }
    puts("PASS: DRV126 0x1555 gate mapping, 65,536 transition cases, OFF preserves CH4");
    return 0;
}
