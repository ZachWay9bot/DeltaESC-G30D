#include <stdint.h>
#include "stm32f103_min.h"
#include "shu_iap.h"

#define APP_BASE_ADDR          0x08001000u
#define APP_MAX_BYTES          (50u * 1024u)
#define IAP_BODY_BYTES         11u
#define IAP_IDLE_COUNTS        120u
#define SYSCLK_HZ              64000000u

#define FLASH_KEY1             0x45670123u
#define FLASH_KEY2             0xCDEF89ABu
#define FLASH_SR_BSY           (1u << 0)
#define FLASH_SR_PGERR         (1u << 2)
#define FLASH_SR_WRPRTERR      (1u << 4)
#define FLASH_SR_EOP           (1u << 5)
#define FLASH_CR_PG            (1u << 0)
#define FLASH_CR_LOCK          (1u << 7)

#define SCB_AIRCR              REG32(0xE000ED0Cu)
#define SCB_AIRCR_VECTKEY      (0x5FAu << 16)
#define SCB_AIRCR_SYSRESETREQ  (1u << 2)

extern volatile uint16_t g_last_abs_current;
extern volatile uint8_t g_power_armed;
void power_stage_force_disarm(void);

typedef struct {
    uint8_t state;
    uint8_t index;
    uint8_t body[IAP_BODY_BYTES];
} iap_rx_t;

static iap_rx_t rx;

static void reset_parser(void) {
    rx.state = 0u;
    rx.index = 0u;
}

static uint16_t checksum16(const uint8_t *p, uint8_t n) {
    uint16_t sum = 0u;
    for (uint8_t i = 0u; i < n; ++i) sum = (uint16_t)(sum + p[i]);
    return (uint16_t)~sum;
}

static uint32_t exact_iap_start(void) {
    const uint8_t len = rx.body[0];
    const uint8_t src = rx.body[1];
    const uint8_t dst = rx.body[2];
    const uint8_t cmd = rx.body[3];
    const uint8_t arg = rx.body[4];

    /* Public G30 tooling uses two LEN conventions for the same 11-byte
       IAP-start frame: 4 payload bytes, or 4 routing + 4 payload bytes. */
    if (len != 4u && len != 8u) return 0u;
    if (dst != 0x20u) return 0u;
    if (src != 0x21u && src != 0x3Eu && src != 0x3Fu) return 0u;
    if ((cmd != 0x02u && cmd != 0x03u) || arg != 0x07u) return 0u;

    const uint16_t fw_size =
        (uint16_t)rx.body[5] | ((uint16_t)rx.body[6] << 8);
    if (fw_size < 256u || (uint32_t)fw_size > APP_MAX_BYTES) return 0u;

    const uint16_t got =
        (uint16_t)rx.body[9] | ((uint16_t)rx.body[10] << 8);
    const uint16_t want = checksum16(rx.body, 9u);
    return (uint32_t)(got == want);
}

uint32_t shu_iap_feed_byte(uint8_t b) {
    switch (rx.state) {
    case 0u:
        if (b == 0x5Au) rx.state = 1u;
        break;

    case 1u:
        if (b == 0xA5u) {
            rx.state = 2u;
            rx.index = 0u;
        } else if (b != 0x5Au) {
            reset_parser();
        }
        break;

    case 2u:
        if (rx.index >= IAP_BODY_BYTES) {
            reset_parser();
            break;
        }

        rx.body[rx.index++] = b;

        if (rx.index == 1u && rx.body[0] != 4u && rx.body[0] != 8u) {
            reset_parser();
            break;
        }

        if (rx.index == IAP_BODY_BYTES) {
            const uint32_t request = exact_iap_start();
            reset_parser();
            return request;
        }
        break;

    default:
        reset_parser();
        break;
    }

    return 0u;
}

static void delay_ms(uint32_t ms) {
    const uint32_t cycles = (SYSCLK_HZ / 1000u) * ms;
    const uint32_t start = DWT_CYCCNT;
    while ((uint32_t)(DWT_CYCCNT - start) < cycles) {}
}

static uint32_t flash_wait_ready(void) {
    uint32_t guard = 4000000u;
    while ((FLASH_SR & FLASH_SR_BSY) && guard) --guard;
    return (uint32_t)(guard != 0u);
}

static uint32_t invalidate_app_vector(void) {
    if (!flash_wait_ready()) return 0u;

    if (FLASH_CR & FLASH_CR_LOCK) {
        FLASH_KEYR = FLASH_KEY1;
        FLASH_KEYR = FLASH_KEY2;
    }
    if (FLASH_CR & FLASH_CR_LOCK) return 0u;

    /* Clear only stale status flags. */
    FLASH_SR = FLASH_SR_EOP | FLASH_SR_PGERR | FLASH_SR_WRPRTERR;

    FLASH_CR |= FLASH_CR_PG;
    REG16(APP_BASE_ADDR + 2u) = 0x0000u;
    const uint32_t ready = flash_wait_ready();
    FLASH_CR &= ~FLASH_CR_PG;

    const uint32_t errors = FLASH_SR & (FLASH_SR_PGERR | FLASH_SR_WRPRTERR);
    FLASH_SR = FLASH_SR_EOP | FLASH_SR_PGERR | FLASH_SR_WRPRTERR;
    FLASH_CR |= FLASH_CR_LOCK;

    if (!ready || errors) return 0u;
    return (uint32_t)(REG16(APP_BASE_ADDR + 2u) == 0x0000u);
}

static void system_reset(void) {
    irq_disable();
    __asm volatile("dsb" ::: "memory");
    SCB_AIRCR = SCB_AIRCR_VECTKEY | SCB_AIRCR_SYSRESETREQ;
    __asm volatile("dsb" ::: "memory");
    for (;;) {}
}

void shu_iap_handoff(void) {
    /* This v0.4.x path has no wheel-speed estimator yet, so the release
       remains bench-only. Refuse to touch flash unless the bridge is fully
       disarmed and the measured phase-current residual is small. */
    power_stage_force_disarm();
    delay_ms(20u);

    if (g_power_armed) return;
    if (g_last_abs_current > IAP_IDLE_COUNTS) return;

    if (!invalidate_app_vector()) return;
    system_reset();
}
