#include <stdint.h>
#include "stm32f103_min.h"
#include "ninebot_diag.h"

#ifndef POWER_STAGE_ARM_ALLOWED
#define POWER_STAGE_ARM_ALLOWED 0
#endif

#define DELTAESC_ADDR       0x20u
#define APP_ADDR            0x3Eu
#define BLE_ADDR            0x21u
#define PC_ADDR             0x3Fu
#define RX_BODY_MAX         64u
#define USART2_BRR_115200   278u /* PCLK1=32 MHz -> 115107 baud, -0.08% */

extern volatile uint32_t g_adc_samples;
extern volatile uint32_t g_control_hard_overruns;
extern volatile uint32_t g_safety_latched;
extern volatile uint32_t g_overcurrent_trips;
extern volatile uint32_t g_sample_interval_min;
extern volatile uint32_t g_sample_interval_max;
extern volatile uint32_t g_last_isr_cycles;
extern volatile uint32_t g_max_isr_cycles;
extern volatile uint32_t g_last_control_cycles;
extern volatile uint32_t g_max_control_cycles;
extern volatile uint16_t g_adc_raw[4];
extern volatile uint16_t g_last_abs_current;
extern volatile uint16_t g_peak_abs_current;
extern volatile uint8_t  g_power_armed;

static uint8_t rx_state;
static uint8_t rx_body[RX_BODY_MAX];
static uint8_t rx_index;
static uint8_t rx_expected;

static void gpio_cfg_nibble_local(uint32_t base, unsigned pin, uint32_t nibble) {
    volatile uint32_t *reg = (volatile uint32_t *)((pin < 8u) ? (base + 0x00u) : (base + 0x04u));
    unsigned shift = (pin & 7u) * 4u;
    uint32_t v = *reg;
    v = (v & ~(0xFu << shift)) | ((nibble & 0xFu) << shift);
    *reg = v;
}

static void put_u16(uint8_t *p, unsigned o, uint16_t v) {
    p[o] = (uint8_t)(v & 0xFFu);
    p[o + 1u] = (uint8_t)(v >> 8);
}

static void put_u32(uint8_t *p, unsigned o, uint32_t v) {
    p[o] = (uint8_t)(v & 0xFFu);
    p[o + 1u] = (uint8_t)((v >> 8) & 0xFFu);
    p[o + 2u] = (uint8_t)((v >> 16) & 0xFFu);
    p[o + 3u] = (uint8_t)((v >> 24) & 0xFFu);
}

static uint16_t checksum(const uint8_t *p, unsigned n) {
    uint16_t sum = 0u;
    for (unsigned i = 0u; i < n; ++i) sum = (uint16_t)(sum + p[i]);
    return (uint16_t)~sum;
}

static void parser_reset(void) {
    rx_state = 0u;
    rx_index = 0u;
    rx_expected = 0u;
}

static uint8_t source_allowed(uint8_t src) {
    return (uint8_t)(src == APP_ADDR || src == BLE_ADDR || src == PC_ADDR);
}

static void tx_frame(uint8_t dst, uint8_t arg, const uint8_t *payload, uint8_t len) {
    uint8_t frame[RX_BODY_MAX + 2u];
    unsigned n = 0u;
    if (len > (RX_BODY_MAX - 9u)) return;

    frame[n++] = 0x5Au;
    frame[n++] = 0xA5u;
    frame[n++] = len;
    frame[n++] = DELTAESC_ADDR;
    frame[n++] = dst;
    frame[n++] = DELTAESC_CMD_CONFIG;
    frame[n++] = arg;
    for (unsigned i = 0u; i < len; ++i) frame[n++] = payload[i];

    uint16_t ck = checksum(&frame[2], 5u + len);
    frame[n++] = (uint8_t)(ck & 0xFFu);
    frame[n++] = (uint8_t)(ck >> 8);

    /* Stock G30 dashboard link is single-wire half duplex on PA2.
       Never transmit while the receiver is enabled, so our own response
       cannot feed back into the parser. */
    USART2_CR1 &= ~USART_CR1_RE;
    for (unsigned i = 0u; i < n; ++i) {
        while (!(USART2_SR & USART_SR_TXE)) {}
        USART2_DR = frame[i];
    }
    while (!(USART2_SR & USART_SR_TC)) {}
    USART2_CR1 |= USART_CR1_RE;
}

static uint8_t status_flags(void) {
    uint8_t f = 0u;
    f |= 1u << 0; /* sensorless branch */
    f |= 1u << 1; /* stock BLE/Ninebot diagnostic path */
#if POWER_STAGE_ARM_ALLOWED
    f |= 1u << 2; /* active-capable build */
#endif
    if (g_power_armed) f |= 1u << 3;
    if (g_safety_latched) f |= 1u << 4;
    return f;
}

static void reply_hello(uint8_t dst) {
    uint8_t p[8];
    p[0] = 1u;              /* DeltaESC private protocol version */
    p[1] = status_flags();
    p[2] = 0u;              /* firmware major */
    p[3] = 4u;              /* firmware minor */
    p[4] = 1u;              /* firmware patch */
    p[5] = 'S';             /* Sensorless */
    p[6] = 'L';             /* Link: stock dashboard */
    p[7] = 'E';             /* Experimental */
    tx_frame(dst, DELTAESC_ARG_HELLO, p, (uint8_t)sizeof p);
}

static void reply_diag(uint8_t dst) {
    uint8_t p[54] = {0};
    uint32_t sample_min = g_sample_interval_min;
    if (sample_min == 0xFFFFFFFFu) sample_min = 0u;

    p[0] = 1u; /* snapshot format */
    p[1] = status_flags();
    put_u16(p, 2u, g_last_abs_current);
    put_u16(p, 4u, g_peak_abs_current);
    put_u32(p, 6u, g_safety_latched);
    put_u32(p,10u, g_adc_samples);
    put_u32(p,14u, sample_min);
    put_u32(p,18u, g_sample_interval_max);
    put_u32(p,22u, g_last_control_cycles);
    put_u32(p,26u, g_max_control_cycles);
    put_u32(p,30u, g_last_isr_cycles);
    put_u32(p,34u, g_max_isr_cycles);
    put_u16(p,38u, g_adc_raw[0]);
    put_u16(p,40u, g_adc_raw[1]);
    put_u16(p,42u, g_adc_raw[2]);
    put_u16(p,44u, g_adc_raw[3]);
    put_u32(p,46u, g_overcurrent_trips);
    put_u32(p,50u, g_control_hard_overruns);
    tx_frame(dst, DELTAESC_ARG_DIAG, p, (uint8_t)sizeof p);
}

static void handle_complete_body(void) {
    uint8_t len = rx_body[0];
    if ((uint8_t)(len + 7u) != rx_expected) return;

    uint16_t got = (uint16_t)rx_body[5u + len] |
                   ((uint16_t)rx_body[6u + len] << 8);
    uint16_t want = checksum(rx_body, 5u + len);
    if (got != want) return;

    uint8_t src = rx_body[1];
    uint8_t dst = rx_body[2];
    uint8_t cmd = rx_body[3];
    uint8_t arg = rx_body[4];

    if (dst != DELTAESC_ADDR || !source_allowed(src) || cmd != DELTAESC_CMD_CONFIG) return;

    if (arg == DELTAESC_ARG_HELLO) {
        reply_hello(src);
    } else if (arg == DELTAESC_ARG_DIAG) {
        reply_diag(src);
    }
}

static void feed_byte(uint8_t b) {
    if (rx_state == 0u) {
        if (b == 0x5Au) rx_state = 1u;
        return;
    }
    if (rx_state == 1u) {
        if (b == 0xA5u) {
            rx_state = 2u;
            rx_index = 0u;
            rx_expected = 0u;
        } else {
            rx_state = (uint8_t)(b == 0x5Au ? 1u : 0u);
        }
        return;
    }

    if (rx_index >= RX_BODY_MAX) {
        parser_reset();
        return;
    }

    rx_body[rx_index++] = b;
    if (rx_index == 1u) {
        uint16_t total = (uint16_t)b + 7u;
        if (total > RX_BODY_MAX || total < 7u) {
            parser_reset();
            return;
        }
        rx_expected = (uint8_t)total;
    }

    if (rx_expected && rx_index == rx_expected) {
        handle_complete_body();
        parser_reset();
    }
}

void ninebot_diag_init(void) {
    RCC_APB1ENR |= (1u << 17); /* USART2 */
    RCC_APB2ENR |= (1u << 2);  /* GPIOA */

    /* PA2 = USART2_TX. In HDSEL mode the receiver also uses this same pin.
       PA3 remains analog phase-A current input and is never touched here. */
    gpio_cfg_nibble_local(GPIOA_BASE, 2u, 0xBu);

    USART2_CR1 = 0u;
    USART2_CR2 = 0u;
    USART2_CR3 = USART_CR3_HDSEL;
    USART2_BRR = USART2_BRR_115200;
    USART2_CR1 = USART_CR1_UE | USART_CR1_TE | USART_CR1_RE;
    parser_reset();

    /* Clear a stale RX/ORE condition left by reset or dashboard boot chatter. */
    if (USART2_SR & (USART_SR_RXNE | USART_SR_ORE)) {
        (void)USART2_SR;
        (void)USART2_DR;
    }
}

void ninebot_diag_poll(void) {
    if (USART2_SR & USART_SR_ORE) {
        (void)USART2_SR;
        (void)USART2_DR;
        parser_reset();
        return;
    }

    while (USART2_SR & USART_SR_RXNE) {
        feed_byte((uint8_t)(USART2_DR & 0xFFu));
    }
}
