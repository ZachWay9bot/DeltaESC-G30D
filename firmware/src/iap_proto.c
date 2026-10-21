#include "iap_proto.h"

#define IAP_HDR0 0x5Au
#define IAP_HDR1 0xA5u
#define IAP_DST_ESC 0x20u

void iap_proto_build_ack(uint8_t dst, uint8_t result, uint8_t out[IAP_ACK_FRAME_LEN]) {
    out[0] = IAP_HDR0;
    out[1] = IAP_HDR1;
    out[2] = 0u;          /* Stock DRV126: ACK has no data payload. */
    out[3] = IAP_DST_ESC; /* ESC is response source. */
    out[4] = dst;         /* Echo original request source as destination. */
    out[5] = IAP_CMD_ACK; /* Stock DRV126 always answers IAP 07..0A with 0x0B. */
    out[6] = result;      /* Result lives in INDEX/ARG when LEN=0. */
    uint16_t sum = 0u;
    for (uint8_t i = 2u; i < 7u; ++i) sum = (uint16_t)(sum + out[i]);
    sum = (uint16_t)~sum;
    out[7] = (uint8_t)sum;
    out[8] = (uint8_t)(sum >> 8);
}


uint8_t iap_proto_effective_write_len(uint32_t expected, uint32_t received,
                                      const uint8_t *data, uint8_t frame_len,
                                      uint8_t *take) {
    if (!data || !take || frame_len == 0u || received >= expected) return 0u;
    uint32_t remaining = expected - received;
    uint8_t n = frame_len;
    if ((uint32_t)n <= remaining) {
        *take = n;
        return 1u;
    }
    /* Stock Ninebot IAP sends a full 128-byte final block and zero-pads
       bytes beyond the advertised file length.  Process only the real bytes
       and reject non-zero tail data rather than silently hiding corruption. */
    uint8_t used = (uint8_t)remaining;
    for (uint8_t i = used; i < n; ++i) if (data[i] != 0u) return 0u;
    *take = used;
    return 1u;
}

uint8_t iap_proto_next_index(uint8_t current) {
    return (uint8_t)(current + 1u); /* stock protocol wraps 0xFF -> 0x00 */
}

uint32_t iap_proto_sum32_update(uint32_t sum, const uint8_t *data, uint8_t len) {
    if (!data) return sum;
    for (uint8_t i = 0u; i < len; ++i) sum += data[i];
    return sum;
}

uint32_t iap_proto_sum32_finish(uint32_t sum) {
    return ~sum;
}
