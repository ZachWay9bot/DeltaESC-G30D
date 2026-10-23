/* Synthetic electrical PMSM plant exercising the actual closed-loop
 * sensorless C logic, no MT8006A outputs. This is NOT hardware validation.
 * Assumption: the rotor perfectly follows the forced electrical field in
 * open loop. That is favorable by design; real launch may slip or stall. */
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include "sensorless_control.h"
#ifndef SENSORLESS_CLOSED_LOOP_ALLOWED
#error "Host plant test must exercise observer handover"
#endif
#define REQUIRE(expr) do{if(!(expr)){fprintf(stderr,"FAIL %s line %d\n",#expr,__LINE__);return 1;}}while(0)
#define PI 3.14159265358979323846
static int16_t counts(double ma) {
    double x=ma/50.35;
    if(x>32767.0)x=32767.0;
    if(x< -32768.0)x=-32768.0;
    return (int16_t)lround(x);
}
static int simulate(uint8_t cut_bemf) {
    sensorless_control_t s={0};
    sensorless_control_init(&s);
    REQUIRE(sensorless_control_set_motor_params(&s,100000u,100000u,15000u));
    sensorless_control_set_drive(&s,1u);
    double current_alpha_ma=0.0,current_beta_ma=0.0;
    double rotor_theta_rad=0.0, electrical_omega_rad_s=0.0;
    uint8_t did_handover=0u,did_close=0u,cut=0u;
    for(uint32_t tick=0u;tick<16000u;tick++) {
        if(s.state==SENSORLESS_CLOSED_LOOP) {
            rotor_theta_rad+=electrical_omega_rad_s/4000.0;
        } else if(s.state==SENSORLESS_OPEN_LOOP || s.state==SENSORLESS_HANDOVER) {
            electrical_omega_rad_s=(double)s.openloop_step*2.0*PI*4000.0/65536.0;
            rotor_theta_rad=(double)s.openloop_phase*2.0*PI/65536.0;
        }
        /* 35V bus, R=0.1 ohm, L=100uH, lambda=0.015 Wb. */
        double emfa=-electrical_omega_rad_s*15.0*sin(rotor_theta_rad);
        double emfb= electrical_omega_rad_s*15.0*cos(rotor_theta_rad);
        if(cut) { emfa=0.0;emfb=0.0; }
        double va=(double)s.v_alpha_prev*35000.0/32768.0;
        double vb=(double)s.v_beta_prev*35000.0/32768.0;
        current_alpha_ma+=(va-0.1*current_alpha_ma/1000.0-emfa)*2.5;
        current_beta_ma +=(vb-0.1*current_beta_ma /1000.0-emfb)*2.5;
        if(current_alpha_ma>10000)current_alpha_ma=10000;
        if(current_alpha_ma< -10000)current_alpha_ma=-10000;
        if(current_beta_ma>10000)current_beta_ma=10000;
        if(current_beta_ma< -10000)current_beta_ma=-10000;
        phase_current_counts_t currents={
            counts(current_alpha_ma),
            counts((-current_alpha_ma+1.7320508075688772*current_beta_ma)/2.0),
            counts((-current_alpha_ma-1.7320508075688772*current_beta_ma)/2.0)
        };
        sensorless_control_step(&s,currents,2500u,1999u,1u);
        if(s.state==SENSORLESS_HANDOVER)did_handover=1u;
        if(s.state==SENSORLESS_CLOSED_LOOP)did_close=1u;
        if(did_close && cut_bemf && !cut)cut=1u;
        if(s.state==SENSORLESS_FAULT) {
            REQUIRE(cut_bemf && cut && did_close && s.fault!=0u);
            REQUIRE(!s.drive_request && s.ccr1==1000u && s.ccr2==1000u && s.ccr3==1000u);
            sensorless_control_set_drive(&s,1u);
            REQUIRE(!s.drive_request); /* latch cannot be bypassed */
            return 0;
        }
    }
    REQUIRE(did_handover && did_close);
    REQUIRE(!cut_bemf && s.state==SENSORLESS_CLOSED_LOOP && s.fault==0u);
    sensorless_control_set_drive(&s,0u);
    REQUIRE(!s.drive_request && s.state==SENSORLESS_STOP);
    REQUIRE(s.ccr1==1000u && s.ccr2==1000u && s.ccr3==1000u);
    return 0;
}
int main(void) {
    if(simulate(0u)||simulate(1u))return 1;
    puts("PASS synthetic sensorless PMSM: align->open-loop->handover->closed-loop, throttle coast, observer-loss same-tick stop");
    return 0;
}
