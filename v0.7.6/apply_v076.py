#!/usr/bin/env python3
from pathlib import Path
root=Path(__file__).resolve().parent
m=root/'src/main.c'
mk=root/'Makefile'

watch=root/'src/motor_bench_watchdog.h'
watch.write_text(r'''#ifndef MOTOR_BENCH_WATCHDOG_H
#define MOTOR_BENCH_WATCHDOG_H
#include <stdint.h>
static inline uint8_t motor_bench_deadline_reached(uint32_t now, uint32_t deadline) {
    return (uint8_t)(((int32_t)(now - deadline)) >= 0);
}
#endif
''')

s=m.read_text()
def one(old,new,label):
    global s
    if s.count(old)!=1:
        raise SystemExit(label+' anchor mismatch: '+str(s.count(old)))
    s=s.replace(old,new,1)

one('#include "iap_proto.h"',
    '#include "iap_proto.h"\n#include "motor_bench_watchdog.h"',
    'include')
one('#define FW_BUILD 0x0750u',
    '#define FW_BUILD 0x0760u',
    'build id')
one('#define ADC_STALE_MS                    5u',
    '#define ADC_STALE_MS                    5u\n#define MOTOR_BENCH_TIMEOUT_MS         1000u',
    'timeout define')
one('static uint8_t g_cfg_dirty;',
    'static uint8_t g_cfg_dirty;\nstatic volatile uint8_t g_motor_bench_active;\nstatic volatile uint32_t g_motor_bench_deadline_ms;\nstatic volatile uint16_t g_motor_bench_timeouts;',
    'globals')
one('g_power_armed=0u; sensorless_control_stop(&ctrl);',
    'g_power_armed=0u;g_motor_bench_active=0u; sensorless_control_stop(&ctrl);',
    'disarm clears timer')

old='''        sensorless_control_set_drive(&ctrl,1u);
  #endif
        action_ack(f,cmd,ACT_OK);return;'''
new='''        sensorless_control_set_drive(&ctrl,1u);
  #endif
        g_motor_bench_active=1u;
        g_motor_bench_deadline_ms=g_ms+MOTOR_BENCH_TIMEOUT_MS;
        action_ack(f,cmd,ACT_OK);return;'''
one(old,new,'E5 deadline')

old='''case 0xD9u: put_u16(p,current_fault_code());put_u16(p+2,diag_flags());put_u16(p+4,HARD_OC_COUNTS);put_u16(p+6,PHASE_SUM_FAULT_COUNTS);n=8u;break;'''
new='''case 0xD9u: put_u16(p,current_fault_code());put_u16(p+2,diag_flags());put_u16(p+4,HARD_OC_COUNTS);put_u16(p+6,PHASE_SUM_FAULT_COUNTS);put_u16(p+8,MOTOR_BENCH_TIMEOUT_MS);put_u16(p+10,g_motor_bench_timeouts);n=12u;break;'''
one(old,new,'D9')

old='''        if(!gate_safety_check())uart_puts("FAULT: PWM safety envelope violated; DISARMED\\r\\n");
        __asm volatile("wfi");'''
new='''        if(g_motor_bench_active && motor_bench_deadline_reached(g_ms,g_motor_bench_deadline_ms)){
            g_motor_bench_active=0u;
            if(g_motor_bench_timeouts!=0xFFFFu)g_motor_bench_timeouts++;
            power_stage_force_disarm();
        }
        if(!gate_safety_check())uart_puts("FAULT: PWM safety envelope violated; DISARMED\\r\\n");
        __asm volatile("wfi");'''
one(old,new,'main-loop watchdog')

s=s.replace('DeltaESC G30D v0.7.5 G30 PROVISIONAL PROFILE',
            'DeltaESC G30D v0.7.6 TIMEBOXED MOTOR BENCH',1)
m.write_text(s)

s=mk.read_text().replace('v0_7_5','v0_7_6')
mk.write_text(s)

test=root/'tools/motor_bench_watchdog_host_test.c'
test.write_text(r'''#include <stdint.h>
#include <stdio.h>
#include "motor_bench_watchdog.h"

int main(void){
    if(motor_bench_deadline_reached(999u,1000u))return 1;
    if(!motor_bench_deadline_reached(1000u,1000u))return 2;
    if(!motor_bench_deadline_reached(1001u,1000u))return 3;
    if(motor_bench_deadline_reached(0xFFFFFFF0u,0x00000010u))return 4;
    if(!motor_bench_deadline_reached(0x00000010u,0x00000010u))return 5;
    if(!motor_bench_deadline_reached(0x00000020u,0x00000010u))return 6;
    puts("PASS v0.7.6 wrap-safe motor bench timeout");
    return 0;
}
''')
print('v0.7.6 timeboxed motor bench overlay applied')
