#pragma once
#include <stdint.h>

typedef enum {
    IAP_IDLE=0, IAP_READY=1, IAP_RECEIVING=2, IAP_COMPLETE=3, IAP_ERROR=4
} iap_state_t;

void iap_update_init(void);
void iap_update_feed(uint8_t byte);
void iap_update_poll(void);
uint8_t iap_update_active(void);
uint8_t iap_update_pending(void);
uint8_t iap_update_state(void);
uint8_t iap_update_last_error(void);
uint32_t iap_update_expected_bytes(void);
uint32_t iap_update_received_bytes(void);
uint32_t iap_update_staged_bytes(void);
