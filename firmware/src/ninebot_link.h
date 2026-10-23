#pragma once
#include <stdint.h>

#define NINEBOT_MAX_PAYLOAD 56u
#define NINEBOT_READ      0x01u
#define NINEBOT_WRITE     0x02u
#define NINEBOT_WRITE_NR  0x03u
#define NINEBOT_READ_ACK  0x04u
#define NINEBOT_WRITE_ACK 0x05u

#define NINEBOT_FMT_55AA  1u
#define NINEBOT_FMT_5AA5  2u

typedef struct {
    uint8_t format;
    uint8_t src;
    uint8_t dst;
    uint8_t cmd;
    uint8_t arg;
    uint8_t payload_len;
    uint8_t payload[NINEBOT_MAX_PAYLOAD];
} ninebot_frame_t;

void ninebot_link_init(void);
void ninebot_link_irq(void);
uint8_t ninebot_link_get_frame(ninebot_frame_t *out);
void ninebot_send_app_response(const ninebot_frame_t *request, uint8_t response_cmd,
                               uint8_t arg, const uint8_t *payload, uint8_t payload_len);
uint32_t ninebot_link_rx_overflows(void);
uint8_t ninebot_link_last_format(void);
void ninebot_link_discard_rx(void);
