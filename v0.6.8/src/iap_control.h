#pragma once
#include <stdint.h>

#define IAP_CONTROL_BASE  0x0801F800u
#define IAP_CONTROL_MAGIC 0x0000505Au
#define IAP_APPLICATION_MAX_BYTES 0x0000C800u

typedef uint8_t (*iap_control_erase_fn)(uint32_t address);
typedef uint8_t (*iap_control_write_fn)(uint32_t address, uint16_t value);
typedef uint32_t (*iap_control_read_fn)(uint32_t address);

/* Every new BEGIN must invalidate any older pending update BEFORE staging is erased. */
uint8_t iap_control_invalidate(iap_control_erase_fn erase_page,
                               iap_control_read_fn read_word);

/* Validity marker at the first halfword is written LAST, after verified metadata. */
uint8_t iap_control_commit(uint32_t image_size,
                           iap_control_write_fn write_half,
                           iap_control_read_fn read_word);
