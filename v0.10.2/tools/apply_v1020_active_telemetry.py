#!/usr/bin/env python3
"""DeltaESC v0.10.2 active-RC telemetry overlay.

Applied after v0.10.0 + v0.10.1. Adds persistent stop snapshots and two read-only
pages so the first real active bench run is diagnosable even after the common
hardware-off path resets the live FOC state to STOP.
"""
from pathlib import Path
import sys

target=Path(sys.argv[1]).resolve()
main=target/'src/main.c'
mpc=target/'src/motor_pipeline.c'
mph=target/'src/motor_pipeline.h'

def one(s,old,new,label):
    n=s.count(old)
    if n!=1:
        raise RuntimeError(f'v0.10.2 {label}: expected one anchor, got {n}: {old[:120]!r}')
    return s.replace(old,new,1)

s=mph.read_text()
s=one(s,
'''    uint8_t neutral_seen;
} motor_pipeline_t;''',
'''    uint8_t neutral_seen;

    /* Persistent snapshot captured after physical gate-off but before the
     * live FOC object is reset to STOP. Read-only diagnostic evidence. */
    uint8_t stop_foc_state;
    uint8_t stop_foc_fault;
    int16_t stop_phase_error;
    int16_t stop_observer_step;
    uint16_t stop_observer_phase;
    uint16_t stop_control_phase;
    int16_t stop_id;
    int16_t stop_iq;
    uint16_t stop_clamp_delta;
    uint16_t stop_rolling_good_ticks;
    uint16_t stop_rolling_iq_ticks;
} motor_pipeline_t;''','pipeline stop snapshot fields')
mph.write_text(s)

s=mpc.read_text()
s=one(s,
'''    if (reason==MOTOR_STOP_ADC || reason==MOTOR_STOP_FAULT ||
        reason==MOTOR_STOP_PWM) p->fault_latched=1u;
    if (p->foc) sensorless_control_stop(p->foc);''',
'''    if (reason==MOTOR_STOP_ADC || reason==MOTOR_STOP_FAULT ||
        reason==MOTOR_STOP_PWM) p->fault_latched=1u;
    if (p->foc) {
        /* Hardware is already OFF above. Preserve the exact observer/control
         * state that caused or preceded the stop before sensorless_control_stop
         * clears the rolling state machine. */
        p->stop_foc_state=p->foc->state;
        p->stop_foc_fault=p->foc->fault;
        p->stop_phase_error=p->foc->phase_error;
        p->stop_observer_step=p->foc->observer_step;
        p->stop_observer_phase=p->foc->observer_phase;
        p->stop_control_phase=p->foc->control_phase;
        p->stop_id=(int16_t)p->foc->id;
        p->stop_iq=(int16_t)p->foc->iq;
        p->stop_clamp_delta=(uint16_t)(p->foc->vesc_flux.clamp_events-
                                       p->foc->rolling_clamp_start);
        p->stop_rolling_good_ticks=p->foc->rolling_good_ticks;
        p->stop_rolling_iq_ticks=p->foc->rolling_iq_ticks;
        sensorless_control_stop(p->foc);
    }''','capture before foc reset')
mpc.write_text(s)

s=main.read_text()
s=one(s,'#define FW_BUILD 0x1010u','#define FW_BUILD 0x1020u','build id')

anchor='''    case 0xFCu: { /* read-only FO70-derived rolling BEMF evidence */'''
if anchor not in s:
    raise RuntimeError('v0.10.2 FC anchor missing')

# Insert after the complete FC case by anchoring the next DA line.
next_anchor='''    case 0xDAu: case 0xDBu: case 0xDCu: case 0xDDu: case 0xDEu: case 0xDFu:
'''
pages=r'''    case 0xFDu: { /* RC12 live/latched observer handover state */
        const uint8_t live=g_motor_pipeline.powered;
        p[0]='R';p[1]='C';p[2]='1';p[3]='2';
        p[4]=live?ctrl.state:g_motor_pipeline.stop_foc_state;
        p[5]=live?ctrl.fault:g_motor_pipeline.stop_foc_fault;
        p[6]=g_power_armed;
        p[7]=g_motor_pipeline.last_stop_reason;
        put_u16(p+8, live?ctrl.observer_phase:g_motor_pipeline.stop_observer_phase);
        put_u16(p+10,live?ctrl.control_phase:g_motor_pipeline.stop_control_phase);
        put_u16(p+12,(uint16_t)(live?ctrl.phase_error:g_motor_pipeline.stop_phase_error));
        put_u16(p+14,(uint16_t)(live?ctrl.observer_step:g_motor_pipeline.stop_observer_step));
        put_u16(p+16,(uint16_t)(live?ctrl.id:g_motor_pipeline.stop_id));
        put_u16(p+18,(uint16_t)(live?ctrl.iq:g_motor_pipeline.stop_iq));
        put_u16(p+20,live?ctrl.rolling_good_ticks:g_motor_pipeline.stop_rolling_good_ticks);
        put_u16(p+22,live?ctrl.rolling_iq_ticks:g_motor_pipeline.stop_rolling_iq_ticks);
        put_u16(p+24,live?(uint16_t)(ctrl.vesc_flux.clamp_events-ctrl.rolling_clamp_start):
                              g_motor_pipeline.stop_clamp_delta);
        put_u16(p+26,(uint16_t)g_last_abs_current);
        n=28u;break;
    }
    case 0xFEu: { /* RC12 current/PWM/ADC fault context */
        const stock_phase_currents_t z=g_motor_pipeline.last_phase;
        uint16_t peak=abs16s(z.ia),q=abs16s(z.ib);
        if(q>peak)peak=q;q=abs16s(z.ic);if(q>peak)peak=q;
        p[0]='P';p[1]='I';p[2]='1';p[3]='2';
        put_u16(p+4,(uint16_t)z.ia);put_u16(p+6,(uint16_t)z.ib);put_u16(p+8,(uint16_t)z.ic);
        put_u16(p+10,peak);
        put_u16(p+12,ctrl.ccr1);put_u16(p+14,ctrl.ccr2);put_u16(p+16,ctrl.ccr3);
        put_u16(p+18,(uint16_t)ctrl.run_iq_counts);
        p[20]=g_motor_pipeline.last_sector;
        p[21]=g_motor_pipeline.adc.expected_sector;
        put_u16(p+22,(uint16_t)g_adc_raw[3]);
        put_u16(p+24,(uint16_t)g_safety_latched);
        p[26]=g_motor_pipeline.fault_latched;
        p[27]=g_gate_kill_latched;
        n=28u;break;
    }
'''
s=one(s,next_anchor,pages+next_anchor,'FD/FE pages')

for req in [
    '#define FW_BUILD 0x1020u',
    "case 0xFDu:",
    "case 0xFEu:",
    "g_motor_pipeline.stop_foc_state",
    "g_motor_pipeline.stop_clamp_delta",
    "g_gate_kill_latched"
]:
    if req not in s:
        raise RuntimeError('v0.10.2 contract missing: '+req)
main.write_text(s)
print('PASS v0.10.2 active telemetry overlay: persistent stop snapshot + FD/FE pages')
