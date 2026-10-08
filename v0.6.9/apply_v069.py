#!/usr/bin/env python3
"""Add PA12 power-button handling to the v0.6.8 audited source, never to a flash ZIP.

Fail closed: any upstream C shape change MUST fail the build, not silently skip a fix.
"""
from pathlib import Path
import re
import sys

root = Path(sys.argv[1]).resolve()

def once(s, old, new, label):
    n=s.count(old)
    if n!=1:
        raise RuntimeError(f"{label}: wanted exactly one marker, got {n}: {old[:80]!r}")
    return s.replace(old,new,1)

def function_replace(s, name, transformer):
    m=re.search(r"\b"+re.escape(name)+r"\s*\([^;]*?\)\s*\{", s, flags=re.S)
    if not m: raise RuntimeError(f"missing C function {name}")
    start=m.end()-1
    depth=0
    for i in range(start,len(s)):
        if s[i]=="{": depth+=1
        elif s[i]=="}":
            depth-=1
            if depth==0:
                return s[:m.start()]+transformer(s[m.start():i+1])+s[i+1:]
    raise RuntimeError(f"unterminated function {name}")

main=root/"src/main.c"
s=main.read_text()
s=once(s,'#include "iap_update.h"','#include "iap_update.h"\n#include "power_logic.h"',"include")
s=once(s,"#define FW_BUILD 0x0607u",'''#define FW_BUILD 0x0609u
#define STOCK_COMPAT_FW_WORD 0x0420u
#define POWER_HOLD_PIN 11u
#define POWER_BUTTON_PIN 12u
#define POWER_BUTTON_LONG_MS 6000u
#define POWER_OFF_ACK_DELAY_MS 150u''',"build/constants")
s=once(s,"volatile uint32_t g_ms;",'''static power_button_state_t g_power_button_state;
static volatile uint8_t g_poweroff_requested;
static volatile uint32_t g_poweroff_deadline_ms;
volatile uint32_t g_ms;''',"power global state")
s=function_replace(s,"power_hold_init",lambda old:'''static void power_hold_init(void) {
    RCC_APB2ENR |= (1u << 0) | (1u << 2) | (1u << 3) | (1u << 4);
    /* The dashboard PA2 (yellow) is UART; PA12 (green) is a button input. */
    gpio_cfg_nibble(GPIOA_BASE, POWER_HOLD_PIN, 0x2u);
    GPIO_BSRR(GPIOA_BASE) = (1u << POWER_HOLD_PIN);
    gpio_cfg_nibble(GPIOA_BASE, POWER_BUTTON_PIN, 0x8u); /* input pullup */
    GPIO_BSRR(GPIOA_BASE) = (1u << POWER_BUTTON_PIN);
}''')
def add_power_functions(old):
    return old+'''
static void schedule_poweroff(uint32_t delay_ms) {
    power_stage_force_disarm();
    g_poweroff_requested=1u;
    g_poweroff_deadline_ms=g_ms+delay_ms;
}
static void power_hold_release_now(void) {
    power_stage_force_disarm();
    GPIO_BRR(GPIOA_BASE) = (1u << POWER_HOLD_PIN);
    for(;;) { __asm volatile("wfi"); }
}
'''
s=function_replace(s,"power_stage_force_disarm",add_power_functions)
def replace_systick(old):
    if "g_ms++" not in old: raise RuntimeError("SysTick clock assumption changed")
    return '''void SysTick_Handler(void) {
    g_ms++;
    const uint8_t line_low=(uint8_t)((GPIO_IDR(GPIOA_BASE)&(1u<<POWER_BUTTON_PIN))==0u);
    if(power_button_step(&g_power_button_state,line_low,(uint8_t)iap_update_active())) {
        g_poweroff_requested=1u;
        g_poweroff_deadline_ms=g_ms;
    }
}'''
s=function_replace(s,"SysTick_Handler",replace_systick)
# Version reads remain stock-shaped; D0 advertises the real DeltaESC build.
s=once(s,"case 0x1Au: put_u16(p,FW_BUILD);n=2u;break;",
'''case 0x1Au: case 0x28u: case 0x66u: case 0x67u: case 0x68u: put_u16(p,STOCK_COMPAT_FW_WORD);n=2u;break;
    case 0x79u: put_u16(p,0u);n=2u;break;''',"stock version reads")
def ack_change(old):
    if "NINEBOT_WRITE" not in old: raise RuntimeError("ACK original changed")
    return '''static void action_ack(const ninebot_frame_t *req,uint8_t cmd,uint8_t st) {
    g_last_action_status=st;
    if(req->cmd==NINEBOT_WRITE_NR)return;
    uint8_t p[1]={st};
    ninebot_send_app_response(req,NINEBOT_WRITE_ACK,cmd,p,1u);
}'''
s=function_replace(s,"action_ack",ack_change)
needle="static void app_write_action(const ninebot_frame_t *f){"
s=once(s,needle,'''static void app_write_power(const ninebot_frame_t *f) {
    uint16_t value=0u;
    if(f->payload_len>=2u)value=get_u16(f->payload);
    else if(f->payload_len>=1u)value=f->payload[0];
    action_ack(f,f->arg,ACT_OK);
    if(value!=0u)schedule_poweroff(POWER_OFF_ACK_DELAY_MS);
}

'''+needle,"power-off register")
s=once(s,"f->cmd==NINEBOT_READ||f->cmd==NINEBOT_WRITE)){",
"f->cmd==NINEBOT_READ||f->cmd==NINEBOT_WRITE||f->cmd==NINEBOT_WRITE_NR)){","read/write accept")
s=once(s,"if(f->cmd==NINEBOT_READ)app_read_response(f);\n        else if(",
"if(f->cmd==NINEBOT_READ)app_read_response(f);\n        else if(f->arg==0x79u)app_write_power(f);\n        else if(","0x79 dispatch")
s=once(s,"sensorless_control_init(&ctrl);","sensorless_control_init(&ctrl);power_button_init(&g_power_button_state,POWER_BUTTON_LONG_MS);","init power state")
# Place the safety check in the main loop, never in the interrupt handler.
needle='__asm volatile("wfi");'
idx=s.rfind(needle)
if idx<0 or s.count(needle)<2: raise RuntimeError("main-loop WFI location changed")
s=s[:idx]+'''if(g_poweroff_requested) {
            /* Cancel an earlier button/0x79 request if any update starts. */
            if(iap_update_active())g_poweroff_requested=0u;
            else if((int32_t)(g_ms-g_poweroff_deadline_ms)>=0)power_hold_release_now();
        }
        '''+s[idx:]
# The C-function extractor starts at the name; keep exactly one return type.
s=once(s,"static void static void power_hold_init","static void power_hold_init","power hold signature")
s=once(s,"void void SysTick_Handler","void SysTick_Handler","SysTick signature")
s=once(s,"static void static void action_ack","static void action_ack","ACK signature")
main.write_text(s)

hdr=root/"src/ninebot_link.h"
h=hdr.read_text()
h=once(h,"#define NINEBOT_WRITE     0x03u\n#define NINEBOT_READ_ACK  0x04u",
'''#define NINEBOT_WRITE     0x02u
#define NINEBOT_WRITE_NR  0x03u
#define NINEBOT_READ_ACK  0x04u
#define NINEBOT_WRITE_ACK 0x05u''',"write protocol constants")
hdr.write_text(h)

mk=root/"Makefile"
m=mk.read_text()
m=once(m,"src/iap_control.c src/dashboard_runtime.c",
"src/iap_control.c src/dashboard_runtime.c src/power_logic.c","makefile sources")
mk.write_text(m)

(root/"src/power_logic.h").write_text('''#pragma once
#include <stdint.h>
typedef struct { uint16_t held_ms; uint16_t threshold_ms; uint8_t fired; } power_button_state_t;
void power_button_init(power_button_state_t *s,uint16_t threshold_ms);
uint8_t power_button_step(power_button_state_t *s,uint8_t active_low_sample,uint8_t inhibit);
''')
(root/"src/power_logic.c").write_text('''#include "power_logic.h"
void power_button_init(power_button_state_t *s,uint16_t threshold_ms){
    s->held_ms=0u;s->threshold_ms=threshold_ms;s->fired=0u;
}
uint8_t power_button_step(power_button_state_t *s,uint8_t line_low,uint8_t inhibit){
    if(inhibit||!line_low){s->held_ms=0u;s->fired=0u;return 0u;}
    if(s->held_ms<s->threshold_ms)s->held_ms++;
    if(!s->fired&&s->held_ms>=s->threshold_ms){s->fired=1u;return 1u;}
    return 0u;
}
''')
(root/"tools/power_logic_host_test.c").write_text('''#include <stdint.h>
#include "power_logic.h"
int main(void){
    power_button_state_t s;power_button_init(&s,6000u);
    for(unsigned i=0;i<5999u;i++)if(power_button_step(&s,1u,0u))return 1;
    if(!power_button_step(&s,1u,0u))return 2;
    for(unsigned i=0;i<10000u;i++)if(power_button_step(&s,1u,0u))return 3;
    if(power_button_step(&s,0u,0u))return 4;
    for(unsigned i=0;i<9000u;i++)if(power_button_step(&s,1u,1u))return 5;
    if(s.held_ms||s.fired)return 6;
    for(unsigned i=0;i<5999u;i++)if(power_button_step(&s,1u,0u))return 7;
    if(!power_button_step(&s,1u,0u))return 8;
    if(power_button_step(&s,1u,1u))return 9;
    return 0;
}
''')
print("v0.6.9 PA12 power integration applied; no flash image packaged")
