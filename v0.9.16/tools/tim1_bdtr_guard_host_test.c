#include "tim1_bdtr_guard.h"
#include <assert.h>
#include <stdio.h>
#include <stdint.h>
int main(void) {
    const uint16_t good=0x8C40u;
    assert(tim1_bdtr_guard_preinit(0u));
    assert(tim1_bdtr_guard_preinit(0x8C40u));
    assert(!tim1_bdtr_guard_preinit(0xCD40u)); /* actual stock idle! */
    assert(!tim1_bdtr_guard_preinit(0x8D40u));
    assert(!tim1_bdtr_guard_preinit(0xCC40u));
    assert(!tim1_bdtr_guard_preinit(0x9C40u));
    assert(!tim1_bdtr_guard_preinit(0xAC40u));
    assert(tim1_bdtr_guard_passive(good,0x40u,0x1000u));
    for(uint32_t i=0;i<65536u;i++) {
        const uint16_t bdtr=(uint16_t)i;
        const uint16_t ccer=(uint16_t)i;
        assert(tim1_bdtr_guard_passive(bdtr,0x40u,0x1000u)==(bdtr==good));
        assert(tim1_bdtr_guard_passive(good,0x40u,ccer)==(ccer==0x1000u));
        assert(tim1_bdtr_guard_six_off(ccer)==((ccer&0x0555u)==0u));
    }
    assert(!tim1_bdtr_guard_passive(0xCD40u,0x40u,0x1000u));
    assert(!tim1_bdtr_guard_passive(0x8C40u,0x40u,0x1555u));
    assert(!tim1_bdtr_guard_passive(0x8C40u,0x40u,0x3000u));
    puts("PASS: TIM1 BDTR 65,536 variants, CCER 65,536 variants, stock 0xCD40 veto");
    return 0;
}
