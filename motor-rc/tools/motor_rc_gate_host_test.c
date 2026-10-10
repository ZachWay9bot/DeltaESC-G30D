/* Motor-RC simulated TIM1 register model.
 * This is not physical safety validation; it checks stop-state invariants.
 */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "tim1_bdtr_guard.h"
#include "tim1_emergency_stop.h"
typedef struct { uint16_t bdtr,ccer; uint8_t killed; } mock_t;
static void normal_coast(mock_t *m) {
    m->ccer=D_GUARD_CH4;
    /* Tim1 MOE stays on solely for the CH4 ADC trigger. */
}
static void terminal_stop(mock_t *m) {
    m->killed=1;
    m->ccer=D_GUARD_CH4;
    m->bdtr=(uint16_t)(m->bdtr&~D_GUARD_MOE);
}
static uint8_t rearm(mock_t *m) {
    if(m->killed ||
       (m->bdtr & (D_GUARD_MOE|D_GUARD_AOE|D_GUARD_BKE|D_GUARD_BKP|D_GUARD_LOCK))!=D_GUARD_MOE)
        return 0u;
    m->ccer=(uint16_t)(D_GUARD_SIX|D_GUARD_CH4);
    return 1u;
}
int main(void) {
    mock_t x={(uint16_t)(D_GUARD_MOE|D_GUARD_OSSR|D_GUARD_OSSI|0x40),
              D_GUARD_CH4,0};
    for(unsigned i=0;i<24u;i++){
        assert(rearm(&x));
        assert(x.ccer==(uint16_t)0x1555u);
        normal_coast(&x);
        assert(tim1_bdtr_guard_passive(x.bdtr,0x40u,x.ccer));
        assert(!x.killed);
    }
    terminal_stop(&x);
    assert(tim1_emergency_registers_off(x.bdtr,x.ccer));
    assert(!rearm(&x));
    normal_coast(&x);
    assert(!rearm(&x));
    for(unsigned bdtr=0u;bdtr<65536u;bdtr++){
        uint8_t p=tim1_bdtr_guard_preinit((uint16_t)bdtr);
        assert(p==(((bdtr&(D_GUARD_AOE|D_GUARD_BKP|D_GUARD_BKE|D_GUARD_LOCK))==0u)?1u:0u));
    }
    puts("PASS: 24 normal E7 coasts rearmed, E6 terminal MOE cleared forever, 65536 register guard states");
    return 0;
}
