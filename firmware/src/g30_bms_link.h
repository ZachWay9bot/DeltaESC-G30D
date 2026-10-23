#pragma once
#include <stdint.h>
#include "g30_bms.h"

#define G30_BMS_USART3_BASE 0x40004800u
#define G30_BMS_IRQ 39u

void g30_bms_link_init(uint32_t now_ms);
void g30_bms_link_irq(uint32_t now_ms);
/* Call after IAP service. Suspended cancels background requests and drains
 * stale buffered data, without changing any IAP/dashboard/motor state. */
void g30_bms_link_poll(uint32_t now_ms,uint8_t suspended);
uint8_t g30_bms_link_soc(uint32_t now_ms);
void g30_bms_link_status(uint32_t now_ms,uint8_t out[G30_BMS_STATUS_BYTES]);
