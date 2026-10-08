#include "motor_config_txn.h"

static uint8_t valid_r(uint32_t v){return (uint8_t)(v>=1000u && v<=2000000u);}
static uint8_t valid_l(uint32_t v){return (uint8_t)(v>=1000u && v<=5000000u);}
static uint8_t valid_flux(uint32_t v){return (uint8_t)(v>=100u && v<=1000000u);}
static uint8_t valid_current(uint16_t v){return (uint8_t)(v>=100u && v<=2000u);}

void motor_config_txn_init(motor_config_txn_t *t, uint16_t default_current_ma){
    t->active.r_uohm=0u;t->active.l_nh=0u;t->active.flux_uwb=0u;
    t->active.phase_offset=0;t->active.test_current_ma=default_current_ma;
    t->pending=t->active;t->pending_mask=0u;t->active_valid=0u;
}
uint8_t motor_config_stage_u32(motor_config_txn_t *t,uint8_t reg,uint32_t value){
    if(reg==0xF0u){if(!valid_r(value))return 0u;t->pending.r_uohm=value;t->pending_mask|=MOTOR_CFG_BIT_R;return 1u;}
    if(reg==0xF1u){if(!valid_l(value))return 0u;t->pending.l_nh=value;t->pending_mask|=MOTOR_CFG_BIT_L;return 1u;}
    if(reg==0xF2u){if(!valid_flux(value))return 0u;t->pending.flux_uwb=value;t->pending_mask|=MOTOR_CFG_BIT_FLUX;return 1u;}
    return 0u;
}
uint8_t motor_config_stage_i16(motor_config_txn_t *t,int16_t value){
    t->pending.phase_offset=value;t->pending_mask|=MOTOR_CFG_BIT_PHASE;return 1u;
}
uint8_t motor_config_stage_u16(motor_config_txn_t *t,uint16_t value){
    if(!valid_current(value))return 0u;
    t->pending.test_current_ma=value;
    t->pending_mask|=MOTOR_CFG_BIT_CURRENT;
    return 1u;
}
uint8_t motor_config_pending_complete(const motor_config_txn_t *t){return (uint8_t)(t->pending_mask==MOTOR_CFG_ALL);}
uint8_t motor_config_commit(motor_config_txn_t *t){
    if(!motor_config_pending_complete(t))return 0u;
    if(!valid_r(t->pending.r_uohm)||!valid_l(t->pending.l_nh)||!valid_flux(t->pending.flux_uwb)||!valid_current(t->pending.test_current_ma))return 0u;
    t->active=t->pending;t->active_valid=1u;t->pending_mask=0u;return 1u;
}
void motor_config_abort(motor_config_txn_t *t){t->pending=t->active;t->pending_mask=0u;}
