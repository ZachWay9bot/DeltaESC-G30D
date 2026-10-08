#pragma once
#include <stdint.h>

/* Feed one raw byte from the stock dashboard/ESC PA2 link.
 * Returns 1 only for an exact, checksum-valid G30 IAP-start request. */
uint32_t shu_iap_feed_byte(uint8_t b);

/* Enter preserved stock IAP recovery after an exact start request.
 * Returns if safety or flash-programming preconditions reject the handoff. */
void shu_iap_handoff(void);
