#!/usr/bin/env python3
from pathlib import Path

root=Path(__file__).resolve().parent
m=root/'src/main.c'
c=root/'src/sensorless_control.c'
mk=root/'Makefile'
hdr=root/'src/stm32f103_min.h'
t=root/'tools/motor_core_host_test.c'

s=m.read_text()

def rep(old,new):
    global s
    if old not in s:
        raise SystemExit('missing main.c pattern: '+old[:80])
    s=s.replace(old,new)

rep('#define ARM_IDLE_CURRENT_COUNTS       120u','#define ARM_IDLE_CURRENT_COUNTS        20u')
rep('#define HARD_OC_COUNTS                700u','#define HARD_OC_COUNTS                100u')
rep('#define ADC_STALE_MS                    5u','#define ADC_STALE_MS                    5u\n#define PHASE_SUM_FAULT_COUNTS          30u')
rep('#define FW_BUILD 0x0700u','#define FW_BUILD 0x0720u')
rep('static uint8_t g_phase_map_valid;','static uint8_t g_phase_map_valid = 1u;')
rep('static uint8_t g_phase_map[3] = {0xFFu,0xFFu,0xFFu};','static uint8_t g_phase_map[3] = {0u,1u,2u};')
rep('static uint8_t g_phase_signs;','static uint8_t g_phase_signs = 0x07u; /* offset-ADC on U/V/W */')
rep('static uint8_t g_phase_direction;','static uint8_t g_phase_direction = 1u; /* direct U/V/W order */')

rep('static void gate_enable_gpio_init(void) { gpio_cfg_nibble(GPIOB_BASE,1,0x2u); GPIO_BRR(GPIOB_BASE)=(1u<<1); }\n','')
rep('    GPIO_BRR(GPIOB_BASE)=(1u<<1); TIM_CCER(TIM1_BASE)&=~GATE_CCER_MASK; gate_pins_to_safe_inputs();\n',
    '    TIM_CCER(TIM1_BASE)&=~GATE_CCER_MASK; gate_pins_to_safe_inputs();\n')
rep('        if (!(GPIO_ODR(GPIOB_BASE)&(1u<<1))) ok=0u;\n','')
rep('        if (GPIO_ODR(GPIOB_BASE)&(1u<<1)) ok=0u;\n','')
rep('    g_power_armed=1u; GPIO_BSRR(GPIOB_BASE)=(1u<<1); if(!gate_safety_check())return 0u; return 1u;\n',
    '    g_power_armed=1u; if(!gate_safety_check())return 0u; return 1u;\n')
rep('irq_disable();clock_64mhz_hsi();dwt_init();power_hold_init();gate_enable_gpio_init();gate_pins_to_safe_inputs();uart1_debug_init();',
    'irq_disable();clock_64mhz_hsi();dwt_init();power_hold_init();gate_pins_to_safe_inputs();uart1_debug_init();')

anchor='static void power_stage_force_disarm(void) {\n'
helper='''static void break_input_init(void) {
    /* G30 reference: PB12 = TIM1_BKIN, active low. */
    RCC_APB2ENR |= (1u << 3);
    gpio_cfg_nibble(GPIOB_BASE,12,0x8u);
    GPIO_BSRR(GPIOB_BASE)=(1u<<12);
}

'''
rep(anchor,helper+anchor)
rep('TIM_BDTR(TIM1_BASE)=DEADTIME_DTG|TIM_BDTR_OSSI|TIM_BDTR_OSSR|TIM_BDTR_MOE;',
    'TIM_BDTR(TIM1_BASE)=DEADTIME_DTG|TIM_BDTR_OSSI|TIM_BDTR_OSSR|TIM_BDTR_BKE|TIM_BDTR_MOE;')
rep('adc1_injected_init();tim1_pwm_and_adc_trigger_init();ninebot_link_init();',
    'adc1_injected_init();break_input_init();tim1_pwm_and_adc_trigger_init();ninebot_link_init();')

rep('''    int32_t ia=(int32_t)s0-(int32_t)g_adc_offset[0],ib=(int32_t)s1-(int32_t)g_adc_offset[1],ic=(int32_t)s2-(int32_t)g_adc_offset[2];
    uint16_t aa=abs16s(ia),ab=abs16s(ib),ac=abs16s(ic),peak=aa;if(ab>peak)peak=ab;if(ac>peak)peak=ac;
''',
'''    /* G30 MCSDK convention: phase current = calibrated offset - ADC sample.
       Public G30 motor-control configuration maps PA3/PA4/PA5 directly to U/V/W. */
    int32_t ia=(int32_t)g_adc_offset[0]-(int32_t)s0;
    int32_t ib=(int32_t)g_adc_offset[1]-(int32_t)s1;
    int32_t ic=(int32_t)g_adc_offset[2]-(int32_t)s2;
    int32_t isum=ia+ib+ic;if(isum<0)isum=-isum;
    if(g_power_armed&&(uint32_t)isum>PHASE_SUM_FAULT_COUNTS){g_safety_latched=0x0C02u;power_stage_force_disarm();}
    uint16_t aa=abs16s(ia),ab=abs16s(ib),ac=abs16s(ic),peak=aa;if(ab>peak)peak=ab;if(ac>peak)peak=ac;
''')
rep('''    if(cmd==0xE5u){
        if(f->payload_len<4u){action_ack(f,cmd,ACT_BAD_RANGE);return;}uint16_t ma=get_u16(f->payload+2);
        if(!sensorless_control_set_run_current_ma(&ctrl,ma)){action_ack(f,cmd,ACT_BAD_RANGE);return;}
''',
'''    if(cmd==0xE5u){
        if(!g_phase_map_valid){action_ack(f,cmd,ACT_UNSAFE);return;}
        if(f->payload_len<4u){action_ack(f,cmd,ACT_BAD_RANGE);return;}uint16_t ma=get_u16(f->payload+2);
        if(ma<100u||ma>500u||!sensorless_control_set_run_current_ma(&ctrl,ma)){action_ack(f,cmd,ACT_BAD_RANGE);return;}
''')
rep('case 0xD9u: put_u16(p,current_fault_code());put_u16(p+2,diag_flags());put_u16(p+4,HARD_OC_COUNTS);n=6u;break;',
    'case 0xD9u: put_u16(p,current_fault_code());put_u16(p+2,diag_flags());put_u16(p+4,HARD_OC_COUNTS);put_u16(p+6,PHASE_SUM_FAULT_COUNTS);n=8u;break;')
rep('DeltaESC G30D v0.7 motor-core development','DeltaESC G30D v0.7.2 MOTOR TEST BENCH')
m.write_text(s)

s=hdr.read_text()
if '#define TIM_BDTR_BKE' not in s:
    if '#define TIM_BDTR_OSSR   (1u << 11)' not in s:
        raise SystemExit('unexpected stm32f103_min.h BDTR block')
    s=s.replace('#define TIM_BDTR_OSSR   (1u << 11)',
                '#define TIM_BDTR_OSSR   (1u << 11)\n#define TIM_BDTR_BKE    (1u << 12)')
hdr.write_text(s)

s=c.read_text()
if '#define ALIGN_IQ_COUNTS 10' not in s or '#define DEFAULT_RUN_CURRENT_MA 500u' not in s:
    raise SystemExit('unexpected sensorless_control.c base')
s=s.replace('#define ALIGN_IQ_COUNTS 10','#define ALIGN_IQ_COUNTS 5')
s=s.replace('#define DEFAULT_RUN_CURRENT_MA 500u','#define DEFAULT_RUN_CURRENT_MA 250u')
s=s.replace('if (current_ma < 100u || current_ma > 2000u) return 0u;',
            'if (current_ma < 100u || current_ma > 500u) return 0u;')
c.write_text(s)

s=mk.read_text().replace('v0_7_0','v0_7_2')
mk.write_text(s)

s=t.read_text()
s=s.replace('assert(s.run_current_ma == 500u);','assert(s.run_current_ma == 250u);')
s=s.replace('sensorless_control_set_run_current_ma(&s, 2000u) == 1u','sensorless_control_set_run_current_ma(&s, 500u) == 1u')
s=s.replace('sensorless_control_set_run_current_ma(&s, 2001u) == 0u','sensorless_control_set_run_current_ma(&s, 501u) == 0u')
s=s.replace('sensorless_control_set_run_current_ma(&s,2000u)==1u','sensorless_control_set_run_current_ma(&s,500u)==1u')
s=s.replace('sensorless_control_set_run_current_ma(&s,2001u)==0u','sensorless_control_set_run_current_ma(&s,501u)==0u')
t.write_text(s)

print('v0.7.2 motor bench transform applied')
