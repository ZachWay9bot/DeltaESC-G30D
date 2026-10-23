#pragma once
#include <stdint.h>

#define G30_BMS_SOC_TIMEOUT_MS 2000u
#define G30_BMS_BYTE_TIMEOUT_MS 25u
#define G30_BMS_STATUS_BYTES 16u

/* Stock SOC register only. No battery configuration writes or activation
 * command, no guessed voltage-to-SOC curve, no motor/IAP permissions. */
typedef struct {
    uint32_t last_soc_ms, last_byte_ms;
    uint16_t accepted, rejected;
    uint8_t soc, seen, pos;
    uint8_t raw[11];
} g30_bms_t;

void g30_bms_init(g30_bms_t *b);
void g30_bms_reset_parser(g30_bms_t *b);
void g30_bms_feed(g30_bms_t *b,uint8_t byte,uint32_t received_ms);
uint8_t g30_bms_soc_valid(const g30_bms_t *b,uint32_t now_ms);
uint8_t g30_bms_soc(const g30_bms_t *b,uint32_t now_ms);
uint8_t g30_bms_read_soc_request(uint8_t out[10]);
void g30_bms_status_payload(const g30_bms_t *b,uint32_t now_ms,
                          uint8_t out[G30_BMS_STATUS_BYTES]);
