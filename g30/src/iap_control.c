#include "iap_control.h"

static uint8_t control_erased(iap_control_read_fn read_word) {
    return read_word(IAP_CONTROL_BASE) == 0xFFFFFFFFu &&
           read_word(IAP_CONTROL_BASE + 4u) == 0xFFFFFFFFu &&
           read_word(IAP_CONTROL_BASE + 8u) == 0xFFFFFFFFu;
}

uint8_t iap_control_invalidate(iap_control_erase_fn erase_page,
                               iap_control_read_fn read_word) {
    if (!erase_page || !read_word) return 0u;
    if (!erase_page(IAP_CONTROL_BASE)) return 0u;
    return control_erased(read_word);
}

uint8_t iap_control_commit(uint32_t image_size,
                           iap_control_write_fn write_half,
                           iap_control_read_fn read_word) {
    if (!write_half || !read_word || image_size < 256u ||
        image_size > IAP_APPLICATION_MAX_BYTES) return 0u;
    /* Must already be invalidated at BEGIN; never erase here after verification. */
    if (!control_erased(read_word)) return 0u;

    /* Write flag + size before touching the 32-bit magic word. */
    if (!write_half(IAP_CONTROL_BASE + 4u, 1u)) return 0u;
    if (!write_half(IAP_CONTROL_BASE + 6u, 0u)) return 0u;
    if (!write_half(IAP_CONTROL_BASE + 8u, (uint16_t)image_size)) return 0u;
    if (!write_half(IAP_CONTROL_BASE + 10u, (uint16_t)(image_size >> 16))) return 0u;
    if (read_word(IAP_CONTROL_BASE + 4u) != 1u ||
        read_word(IAP_CONTROL_BASE + 8u) != image_size) return 0u;

    /* The lower 16 bits complete exact magic 0x0000505A only at the end.
       Power loss before this last step cannot leave a valid update marker. */
    if (!write_half(IAP_CONTROL_BASE + 2u, 0u)) return 0u;
    if (!write_half(IAP_CONTROL_BASE, (uint16_t)IAP_CONTROL_MAGIC)) return 0u;
    return read_word(IAP_CONTROL_BASE) == IAP_CONTROL_MAGIC &&
           read_word(IAP_CONTROL_BASE + 4u) == 1u &&
           read_word(IAP_CONTROL_BASE + 8u) == image_size;
}
