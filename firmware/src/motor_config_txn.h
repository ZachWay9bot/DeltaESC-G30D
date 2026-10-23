#pragma once
#include <stdint.h>

#define MOTOR_CFG_BIT_R       0x01u
#define MOTOR_CFG_BIT_L       0x02u
#define MOTOR_CFG_BIT_FLUX    0x04u
#define MOTOR_CFG_BIT_PHASE   0x08u
#define MOTOR_CFG_BIT_CURRENT 0x10u
#define MOTOR_CFG_ALL         0x1Fu

typedef struct {
    uint32_t r_uohm;
    uint32_t l_nh;
    uint32_t flux_uwb;
    int16_t phase_offset;
    uint16_t test_current_ma;
} motor_config_values_t;

typedef struct {
    motor_config_values_t active;
    motor_config_values_t pending;
    uint8_t pending_mask;
    uint8_t active_valid;
} motor_config_txn_t;

void motor_config_txn_init(motor_config_txn_t *t, uint16_t default_current_ma);
uint8_t motor_config_stage_u32(motor_config_txn_t *t, uint8_t reg, uint32_t value);
uint8_t motor_config_stage_i16(motor_config_txn_t *t, int16_t value);
uint8_t motor_config_stage_u16(motor_config_txn_t *t, uint16_t value);
uint8_t motor_config_pending_complete(const motor_config_txn_t *t);
uint8_t motor_config_commit(motor_config_txn_t *t);
void motor_config_abort(motor_config_txn_t *t);
