#!/usr/bin/env python3
from pathlib import Path
import sys

root = Path(sys.argv[1]) if len(sys.argv) > 1 else Path('.')

# Makefile
p=root/'Makefile'
s=p.read_text()
s=s.replace('SRC = src/main.c src/sensorless_control.c src/ninebot_link.c src/iap_update.c src/iap_proto.c',
            'SRC = src/main.c src/sensorless_control.c src/ninebot_link.c src/iap_update.c src/iap_proto.c src/power_logic.c')
s=s.replace('v0_6_5','v0_6_6').replace('v0.6.5','v0.6.6')
p.write_text(s)

# Ninebot write semantics.
p=root/'src/ninebot_link.h'
s=p.read_text()
s=s.replace('#define NINEBOT_WRITE     0x03u\n#define NINEBOT_READ_ACK  0x04u',
'''#define NINEBOT_WRITE     0x02u
#define NINEBOT_WRITE_NR  0x03u
#define NINEBOT_READ_ACK  0x04u
#define NINEBOT_WRITE_ACK 0x05u''')
p.write_text(s)

# Main power/identity fix.
p=root/'src/main.c'
s=p.read_text()
s=s.replace('#include "iap_update.h"\n', '#include "iap_update.h"\n#include "power_logic.h"\n',1)
s=s.replace('#define FW_BUILD 0x0605u\n#define OFFSET_CAL_SAMPLES 512u', '''#define FW_BUILD 0x0606u
#define STOCK_COMPAT_FW_WORD 0x0420u
#define POWER_HOLD_RELEASE_MS 6000u
#define POWER_HOLD_PIN             11u
#define POWER_BUTTON_PIN           12u
#define POWER_OFF_ACK_DELAY_MS 150u
#define OFFSET_CAL_SAMPLES 512u''',1)
s=s.replace('static volatile uint8_t g_shu_handoff_refused;\n', '''static volatile uint8_t g_shu_handoff_refused;
static power_button_state_t g_power_button_state;
static volatile uint8_t g_poweroff_requested;
static volatile uint32_t g_poweroff_deadline_ms;
''',1)
s=s.replace('''static void power_hold_init(void) {
    RCC_APB2ENR |= (1u << 0) | (1u << 2) | (1u << 3) | (1u << 4);
    gpio_cfg_nibble(GPIOA_BASE, 11, 0x2u); GPIO_BSRR(GPIOA_BASE) = (1u << 11);
}''','''static void power_hold_init(void) {
    RCC_APB2ENR |= (1u << 0) | (1u << 2) | (1u << 3) | (1u << 4);
    /* G30 Gen1 stock split:
       PA11 = ESC power hold / keep-alive.
       PA12 = physical dashboard power-button input (green wire, active low).
       PA2  = dashboard UART (yellow wire) and must never be sampled as a button. */
    gpio_cfg_nibble(GPIOA_BASE, POWER_HOLD_PIN, 0x2u);
    GPIO_BSRR(GPIOA_BASE) = (1u << POWER_HOLD_PIN);
    gpio_cfg_nibble(GPIOA_BASE, POWER_BUTTON_PIN, 0x8u);
    GPIO_BSRR(GPIOA_BASE) = (1u << POWER_BUTTON_PIN);
}''',1)
needle='''static void power_stage_force_disarm(void) {
    GPIO_BRR(GPIOB_BASE)=(1u<<1); TIM_CCER(TIM1_BASE)&=~GATE_CCER_MASK; gate_pins_to_safe_inputs();
    g_power_armed=0u; sensorless_control_stop(&ctrl);
}
'''
insert=needle+'''
static void schedule_poweroff(uint32_t delay_ms) {
    power_stage_force_disarm();
    g_offset_cal_active=0u;
    g_poweroff_requested=1u;
    g_poweroff_deadline_ms=g_ms+delay_ms;
}

static void power_hold_release_now(void) {
    power_stage_force_disarm();
    GPIO_BRR(GPIOA_BASE)=(1u<<POWER_HOLD_PIN);
    for(;;){__asm volatile("wfi");}
}
'''
if needle not in s: raise SystemExit('missing disarm insertion point')
s=s.replace(needle,insert,1)
s=s.replace('void SysTick_Handler(void){g_ms++;}', '''void SysTick_Handler(void){
    g_ms++;
    uint8_t line_low=(uint8_t)((GPIO_IDR(GPIOA_BASE)&(1u<<POWER_BUTTON_PIN))==0u);
    if(power_button_step(&g_power_button_state,line_low,iap_update_active())){
        g_poweroff_requested=1u;
        g_poweroff_deadline_ms=g_ms;
    }
}''',1)
s=s.replace('case 0x1Au: put_u16(p,FW_BUILD);n=2u;break;', 'case 0x1Au: case 0x28u: case 0x66u: case 0x67u: case 0x68u: put_u16(p,STOCK_COMPAT_FW_WORD);n=2u;break;\n    case 0x79u: put_u16(p,0u);n=2u;break;',1)
s=s.replace('static void action_ack(const ninebot_frame_t *req,uint8_t cmd,uint8_t st){uint8_t p[1]={st};g_last_action_status=st;ninebot_send_app_response(req,NINEBOT_WRITE,cmd,p,1u);}', '''static void action_ack(const ninebot_frame_t *req,uint8_t cmd,uint8_t st){
    g_last_action_status=st;
    if(req->cmd==NINEBOT_WRITE_NR)return;
    uint8_t p[1]={st};ninebot_send_app_response(req,NINEBOT_WRITE_ACK,cmd,p,1u);
}''',1)
mark='static void app_write_action(const ninebot_frame_t *f){'
power='''static void app_write_power(const ninebot_frame_t *f){
    uint16_t value=0u;
    if(f->payload_len>=2u)value=get_u16(f->payload);
    else if(f->payload_len>=1u)value=f->payload[0];
    if(value==0u){action_ack(f,f->arg,ACT_OK);return;}
    action_ack(f,f->arg,ACT_OK);
    schedule_poweroff(POWER_OFF_ACK_DELAY_MS);
}

'''
if mark not in s: raise SystemExit('missing action marker')
s=s.replace(mark,power+mark,1)
s=s.replace('if(f->dst==0x20u&&(f->cmd==NINEBOT_READ||f->cmd==NINEBOT_WRITE)){', 'if(f->dst==0x20u&&(f->cmd==NINEBOT_READ||f->cmd==NINEBOT_WRITE||f->cmd==NINEBOT_WRITE_NR)){',1)
s=s.replace('if(f->cmd==NINEBOT_READ)app_read_response(f);\n        else if(f->arg>=0xE0u', 'if(f->cmd==NINEBOT_READ)app_read_response(f);\n        else if(f->arg==0x79u)app_write_power(f);\n        else if(f->arg>=0xE0u',1)
s=s.replace('v0.6.5 SAFE first validates framing + read route before dashboard-drive semantics.', 'v0.6.6 SAFE keeps motor semantics disabled; power-off and stock app identity are implemented explicitly.',1)
s=s.replace('uart_puts("v0.6.5 BLE arm=")', 'uart_puts("v0.6.6 BLE arm=")',1)
s=s.replace('sensorless_control_init(&ctrl);\n    adc1_injected_init()', 'sensorless_control_init(&ctrl);power_button_init(&g_power_button_state,POWER_HOLD_RELEASE_MS);\n    adc1_injected_init()',1)
s=s.replace('DeltaESC G30D v0.6.5 G30-framing + staged-IAP firmware', 'DeltaESC G30D v0.6.6 power-recovery + stock-identity firmware',1)
s=s.replace('PA2 USART2 115200 half-duplex; G30 5A A5 + compatibility 55 AA; D0-D9/E0-E6/F0-F5; staged IAP enabled.', 'PA2/yellow USART2 115200 half-duplex; PA12/green power button; PA11 power hold; stock 0x02/0x03 writes + 0x05 ACK; 0x79/6s-hold power-off; staged IAP enabled.',1)
s=s.replace('if(!gate_safety_check())uart_puts("FAULT: PWM safety envelope violated; DISARMED\\r\\n");\n        __asm volatile("wfi");', 'if(!gate_safety_check())uart_puts("FAULT: PWM safety envelope violated; DISARMED\\r\\n");\n        if(g_poweroff_requested&&!iap_update_active()&&(int32_t)(g_ms-g_poweroff_deadline_ms)>=0)power_hold_release_now();\n        __asm volatile("wfi");',1)
p.write_text(s)

(root/'src/power_logic.h').write_text('''#pragma once
#include <stdint.h>

typedef struct {
    uint16_t low_ms;
    uint16_t threshold_ms;
} power_button_state_t;

void power_button_init(power_button_state_t *s, uint16_t threshold_ms);
uint8_t power_button_step(power_button_state_t *s, uint8_t line_low, uint8_t inhibit);
''')
(root/'src/power_logic.c').write_text('''#include "power_logic.h"

void power_button_init(power_button_state_t *s, uint16_t threshold_ms) {
    s->low_ms = 0u;
    s->threshold_ms = threshold_ms;
}

uint8_t power_button_step(power_button_state_t *s, uint8_t line_low, uint8_t inhibit) {
    if (inhibit || !line_low) {
        s->low_ms = 0u;
        return 0u;
    }
    if (s->low_ms < s->threshold_ms) s->low_ms++;
    return (uint8_t)(s->low_ms >= s->threshold_ms);
}
''')
(root/'tools/power_logic_host_test.c').write_text('''#include <stdint.h>
#include "power_logic.h"
int main(void){
    power_button_state_t s; power_button_init(&s,6000u);
    for(unsigned i=0;i<5999u;i++) if(power_button_step(&s,1u,0u)) return 1;
    if(!power_button_step(&s,1u,0u)) return 2;
    if(!power_button_step(&s,1u,0u)) return 3;
    if(power_button_step(&s,0u,0u)) return 4;
    if(s.low_ms!=0u) return 5;
    for(unsigned i=0;i<7000u;i++) if(power_button_step(&s,1u,1u)) return 6;
    if(s.low_ms!=0u) return 7;
    for(unsigned i=0;i<5999u;i++) if(power_button_step(&s,1u,0u)) return 8;
    if(!power_button_step(&s,1u,0u)) return 9;
    return 0;
}
''')
(root/'tools/test_power_logic.py').write_text('''#!/usr/bin/env python3
import os,subprocess,tempfile,pathlib
root=pathlib.Path(__file__).resolve().parents[1]
cc=os.environ.get('HOST_CC','cc')
with tempfile.TemporaryDirectory() as td:
    exe=pathlib.Path(td)/'power_logic_test'
    subprocess.check_call([cc,'-std=c11','-Wall','-Wextra','-Werror','-I',str(root/'src'),str(root/'tools/power_logic_host_test.c'),str(root/'src/power_logic.c'),'-o',str(exe)])
    subprocess.check_call([str(exe)])
print('power logic host test: PASS')
''')
(root/'tools/preflight_v066.py').write_text('''#!/usr/bin/env python3
from pathlib import Path
root=Path(__file__).resolve().parents[1]
main=(root/'src/main.c').read_text(); hdr=(root/'src/ninebot_link.h').read_text(); pwr=(root/'src/power_logic.c').read_text(); mk=(root/'Makefile').read_text()
assert '#define FW_BUILD 0x0606u' in main
assert '#define STOCK_COMPAT_FW_WORD 0x0420u' in main
assert 'case 0x1Au: case 0x28u: case 0x66u: case 0x67u: case 0x68u: put_u16(p,STOCK_COMPAT_FW_WORD)' in main
assert "p[0]='D';p[1]='E';p[2]='S';p[3]='C'" in main
assert '#define NINEBOT_WRITE     0x02u' in hdr and '#define NINEBOT_WRITE_NR  0x03u' in hdr and '#define NINEBOT_WRITE_ACK 0x05u' in hdr
assert 'f->arg==0x79u' in main and 'schedule_poweroff(POWER_OFF_ACK_DELAY_MS)' in main
assert '#define POWER_HOLD_PIN             11u' in main and '#define POWER_BUTTON_PIN           12u' in main
assert 'GPIO_IDR(GPIOA_BASE)&(1u<<POWER_BUTTON_PIN)' in main and 'GPIO_IDR(GPIOA_BASE)&(1u<<2)' not in main
assert 'gpio_cfg_nibble(GPIOA_BASE, POWER_BUTTON_PIN, 0x8u)' in main
assert 'GPIO_BSRR(GPIOA_BASE) = (1u << POWER_BUTTON_PIN)' in main
assert 'GPIO_BRR(GPIOA_BASE)=(1u<<POWER_HOLD_PIN)' in main
assert 'iap_update_active()' in main and 'POWER_HOLD_RELEASE_MS 6000u' in main and 'threshold_ms' in pwr
assert 'POWER_STAGE_ARM_ALLOWED=0' in mk and 'SENSORLESS_RUN_ALLOWED=0' in mk
for name in ['DeltaESC_G30D_v0_6_6_ble_syncsafe.bin','DeltaESC_G30D_v0_6_6_ble_observer_zero_vector.bin','DeltaESC_G30D_v0_6_6_ble_sensorless_bench.bin']:
    b=(root/'build'/name).read_bytes(); assert len(b)<=0xD000,(name,len(b))
print('v0.6.6 power/identity preflight: PASS')
''')
p=root/'tools/test_shu_package.py'
s=p.read_text().replace('v0_6_5','v0_6_6').replace('v0.6.5','v0.6.6')
p.write_text(s)

(root/'README.md').write_text('''# DeltaESC G30D v0.6.6 — power recovery + stock identity

Status: **CI/recovery candidate only. Do not flash until the current scooter is restored to stock and this build passes a separate SAFE-only bench.**

v0.6.6 fixes the faults exposed by the first real-controller v0.6.5 test.

- Yellow dashboard wire -> PA2 USART2 half-duplex data only.
- Green dashboard wire -> PA12 physical power-button input, active-low.
- PA11 -> ESC power-hold / keep-alive.
- Continuous ~6 s PA12 LOW requests safe shutdown; IAP activity inhibits shutdown.
- Register 0x79 write non-zero ACKs first, then releases PA11 after 150 ms.
- Ninebot writes corrected to 0x02 write, 0x03 write-no-reply, 0x05 ACK.
- Standard identity reads expose stock-compatible 0x0420; D0 remains DESC + build 0x0606.
- SYNC-SAFE remains POWER_STAGE_ARM_ALLOWED=0 and SENSORLESS_RUN_ALLOWED=0.

This build does not claim full stock dashboard feature compatibility yet. Short/double presses and stock 0x64/0x65 drive semantics remain outside the recovery test.
''')
print('v0.6.6 overlay applied')
