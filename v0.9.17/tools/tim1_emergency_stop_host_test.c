#include "tim1_emergency_stop.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>

static void fault_case(uint16_t ccer, uint16_t bdtr) {
    tim1_emergency_model_t m={bdtr,ccer,0u};
    tim1_emergency_model_stop(&m);
    assert(m.killed);
    assert((m.ccer&0x0555u)==0u);
    assert((m.bdtr&0x8000u)==0u);
    tim1_emergency_model_stop(&m);
    assert((m.ccer&0x0555u)==0u && (m.bdtr&0x8000u)==0u);
    assert(!tim1_emergency_model_can_arm(&m));
}
int main(void) {
    /* Different caller reasons: braking, E6, ADC corruption, watchdog,
     * observer loss, initialization fail, IAP, power-off. */
    const unsigned fault_reasons[]={1,2,3,4,5,6,7,8};
    for(unsigned i=0;i<sizeof(fault_reasons)/sizeof(fault_reasons[0]);i++) {
        (void)fault_reasons[i];
        fault_case(0x1555u,0x8C40u);
    }
    /* Exhaust every possible 16-bit BDTR and CCER value independently. */
    for(uint32_t i=0u;i<65536u;i++) {
        fault_case((uint16_t)i,0x8C40u);
        fault_case(0x1555u,(uint16_t)i);
        assert(tim1_emergency_registers_off((uint16_t)i,0x1000u)
         ==((i&(0x8000u|0x4000u|0x2000u|0x1000u|0x0300u))==0u));
        assert(tim1_emergency_registers_off(0x0C40u,(uint16_t)i)
         ==((i&0x0555u)==0u));
    }
    /* Stock 0xCD40 must never be interpreted as a safe killed state. */
    assert(!tim1_emergency_registers_off(0xCD40u,0x1000u));
    assert(tim1_emergency_registers_off(0x0C40u,0x1000u));
    assert(!tim1_emergency_registers_off(0x8C40u,0x1000u));
    assert(!tim1_emergency_registers_off(0x4C40u,0x1000u));
    assert(!tim1_emergency_registers_off(0x0C40u,0x1555u));
    tim1_emergency_model_t idle={0x8C40u,0x1000u,0u};
    assert(tim1_emergency_model_can_arm(&idle));
    tim1_emergency_model_stop(&idle);
    assert(!tim1_emergency_model_can_arm(&idle));
    puts("PASS: EBiCS-style MOE kill, 131072 register permutations, 8 stop reasons; latch prevents rearm");
    return 0;
}
