#ifndef STOCK_TIM1_GATE_H
#define STOCK_TIM1_GATE_H
#include <stdint.h>
/* Verified in DRV126 SVPWM output routine 0x08005510 (CCER store
 * at 0x0800571A): 0x1555 enables CC1/1N, CC2/2N, CC3/3N and CC4.
 * Gate-drive topology does NOT depend on Hall vs observer rotor angle.
 * PB1 is not controlled by this interface.
 */
#define STOCK_TIM1_GATE_CH_MASK 0x0555u
#define STOCK_TIM1_GATE_ADC_CC4E 0x1000u
#define STOCK_TIM1_GATE_RUN_CCER 0x1555u
#define STOCK_TIM1_GATE_DT_RESERVED 0x0000u
uint16_t stock_tim1_gate_ccer_off(uint16_t previous);
uint16_t stock_tim1_gate_ccer_arm(uint16_t previous);
uint8_t stock_tim1_gate_ccer_is_armed(uint16_t ccer);
uint8_t stock_tim1_gate_ccer_is_off(uint16_t ccer);
#endif
