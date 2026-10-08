#include <stdint.h>
#include "motor_probe.h"
int main(void){
    volatile uint16_t offsets[3]={1000,1001,1002};
    motor_probe_init();
    if(g_motor_probe.magic!=MOTOR_PROBE_MAGIC||g_motor_probe.sequence!=0u)return 1;
    for(unsigned i=0;i<MOTOR_PROBE_WINDOW-1u;i++){
        motor_probe_capture((uint16_t)(1000+i%4),(uint16_t)(1001+i%4),
                           (uint16_t)(1002+i%4),2000u,offsets,100u,3900u,4100u,
                           100u,0u,0x303141u,0x8001u,0x1000u,0x8c40u);
    }
    if(g_motor_probe.windows||g_motor_probe.sequence)return 2;
    motor_probe_capture(1000u,1001u,1002u,2000u,offsets,101u,3900u,4100u,
                        100u,0u,0x303141u,0x8001u,0x1000u,0x8c40u);
    if(g_motor_probe.windows!=1u||g_motor_probe.total_samples!=256u)return 3;
    if(g_motor_probe.sequence!=2u||g_motor_probe.window_size!=256u)return 4;
    if(g_motor_probe.mean_adc[0]!=1001u||g_motor_probe.mean_adc[1]!=1002u||
       g_motor_probe.mean_adc[2]!=1003u||g_motor_probe.mean_adc[3]!=2000u)return 5;
    if(g_motor_probe.min_adc[0]!=1000u||g_motor_probe.max_adc[0]!=1003u)return 6;
    if(g_motor_probe.offset_adc[2]!=1002u||g_motor_probe.gate_armed)return 7;
    if(g_motor_probe.sample_period_min_cycles!=3900u||g_motor_probe.tim1_ccer!=0x1000u)return 8;
    for(unsigned i=0u;i<256u;i++){
        motor_probe_capture(2000u,2000u,2000u,3000u,offsets,123u,4000u,4000u,
                           500u,0u,0u,0u,0x1000u,0u);
    }
    if(g_motor_probe.windows!=2u||g_motor_probe.sequence!=4u)return 9;
    if(g_motor_probe.mean_adc[0]!=2000u||g_motor_probe.min_adc[0]!=2000u||
       g_motor_probe.max_adc[0]!=2000u||g_motor_probe.mean_adc[3]!=3000u)return 10;
    return 0;
}
