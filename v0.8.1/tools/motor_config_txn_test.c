#include <stdint.h>
#include "motor_config_txn.h"
int main(void){
    motor_config_txn_t t;motor_config_txn_init(&t,500u);
    if(t.pending_mask||t.active_valid||t.active.test_current_ma!=500u)return 1;
    if(motor_config_stage_u32(&t,0xF0u,999u))return 2;
    if(!motor_config_stage_u32(&t,0xF0u,250000u))return 3;
    if(!motor_config_stage_u32(&t,0xF1u,180000u))return 4;
    if(!motor_config_stage_u32(&t,0xF2u,12000u))return 5;
    if(!motor_config_stage_i16(&t,-1234))return 6;
    if(motor_config_pending_complete(&t))return 7;
    if(!motor_config_stage_u16(&t,350u))return 8;
    if(!motor_config_pending_complete(&t))return 9;
    if(!motor_config_commit(&t))return 10;
    if(!t.active_valid||t.pending_mask)return 11;
    if(t.active.r_uohm!=250000u||t.active.l_nh!=180000u||t.active.flux_uwb!=12000u)return 12;
    if(t.active.phase_offset!=-1234||t.active.test_current_ma!=350u)return 13;
    if(!motor_config_stage_u32(&t,0xF0u,260000u))return 14;
    motor_config_abort(&t);
    if(t.pending_mask||t.pending.r_uohm!=250000u)return 15;
    if(motor_config_stage_u16(&t,99u)||motor_config_stage_u16(&t,2001u))return 16;
    return 0;
}
