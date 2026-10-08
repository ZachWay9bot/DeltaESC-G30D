#!/usr/bin/env python3
from pathlib import Path
import re

root=Path(__file__).resolve().parent
c=root/'src/sensorless_control.c'
m=root/'src/main.c'
mk=root/'Makefile'

s=c.read_text()
old='s->motor_r_uohm = 90000u; s->motor_l_nh = 100000u; s->flux_uwb = 1800u;'
new='s->motor_r_uohm = 170000u; s->motor_l_nh = 312000u; s->flux_uwb = 1800u;'
if s.count(old)!=1:
    raise SystemExit('sensorless default motor profile anchor mismatch')
s=s.replace(old,new,1)
c.write_text(s)

s=m.read_text()
if '#define FW_BUILD 0x0740u' not in s:
    raise SystemExit('v0.7.4 build identity missing')
s=s.replace('#define FW_BUILD 0x0740u','#define FW_BUILD 0x0750u',1)

s,n=re.subn(r'g_cfg_r_uohm\s*=\s*90000u', 'g_cfg_r_uohm=170000u', s, count=1)
if n!=1: raise SystemExit('main R default anchor mismatch')
s,n=re.subn(r'g_cfg_l_nh\s*=\s*100000u', 'g_cfg_l_nh=312000u', s, count=1)
if n!=1: raise SystemExit('main L default anchor mismatch')

s=s.replace('DeltaESC G30D v0.7.4 OBSERVER HANDOVER',
            'DeltaESC G30D v0.7.5 G30 PROVISIONAL PROFILE',1)
m.write_text(s)

s=mk.read_text().replace('v0_7_4','v0_7_5')
mk.write_text(s)

test=root/'tools/g30_profile_host_test.c'
test.write_text(r'''#include <stdint.h>
#include <stdio.h>
#include "sensorless_control.h"

int main(void){
    sensorless_control_t s;
    sensorless_control_init(&s);
    if(s.motor_r_uohm!=170000u)return 1;
    if(s.motor_l_nh!=312000u)return 2;
    if(s.flux_uwb!=1800u)return 3;
    if(!sensorless_control_set_motor_params(&s,170000u,312000u,1800u))return 4;
    if(sensorless_control_set_motor_params(&s,999u,312000u,1800u))return 5;
    if(sensorless_control_set_motor_params(&s,170000u,999u,1800u))return 6;
    puts("PASS v0.7.5 published G30 R/L defaults; flux remains provisional");
    return 0;
}
''')
print('v0.7.5 provisional G30 R/L profile applied')
