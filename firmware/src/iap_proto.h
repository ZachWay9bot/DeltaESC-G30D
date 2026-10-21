#ifndef IAP_PROTO_H
#define IAP_PROTO_H
#include <stdint.h>
#define IAP_CMD_ACK 0x0Bu
#define IAP_ACK_FRAME_LEN 9u
void iap_proto_build_ack(uint8_t dst, uint8_t result, uint8_t out[IAP_ACK_FRAME_LEN]);
uint8_t iap_proto_effective_write_len(uint32_t expected, uint32_t received,
                                      const uint8_t *data, uint8_t frame_len,
                                      uint8_t *take);
uint8_t iap_proto_next_index(uint8_t current);
uint32_t iap_proto_sum32_update(uint32_t sum, const uint8_t *data, uint8_t len);
uint32_t iap_proto_sum32_finish(uint32_t sum);
#endif
