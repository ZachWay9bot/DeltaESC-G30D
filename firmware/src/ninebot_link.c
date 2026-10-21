#include "ninebot_link.h"
#include "stm32f103_min.h"
#include "iap_update.h"

#define RX_RING_SIZE 256u
#define RX_RING_MASK (RX_RING_SIZE - 1u)
#define USART2_IRQ 38u

static volatile uint8_t rx_ring[RX_RING_SIZE];
static volatile uint16_t rx_head;
static volatile uint16_t rx_tail;
static volatile uint32_t rx_overflow_count;
static volatile uint8_t last_format;

static void gpio_cfg_nibble_local(uint32_t base, unsigned pin, uint32_t nibble) {
    volatile uint32_t *reg = (volatile uint32_t *)((pin < 8u) ? (base + 0x00u) : (base + 0x04u));
    unsigned shift = (pin & 7u) * 4u;
    uint32_t v = *reg;
    v = (v & ~(0xFu << shift)) | ((nibble & 0xFu) << shift);
    *reg = v;
}

void ninebot_link_init(void) {
    RCC_APB2ENR |= (1u << 0) | (1u << 2);
    RCC_APB1ENR |= (1u << 17);
    gpio_cfg_nibble_local(GPIOA_BASE, 2, 0xEu); /* USART2 AF open-drain, stock yellow wire */
    USART_CR1_REG(USART2_BASE) = 0u;
    USART_CR2_REG(USART2_BASE) = 0u;
    USART_CR3_REG(USART2_BASE) = USART_CR3_HDSEL;
    USART_BRR_REG(USART2_BASE) = 278u; /* APB1 32 MHz / 115200 */
    USART_CR1_REG(USART2_BASE) = USART_CR1_UE | USART_CR1_RE | USART_CR1_RXNEIE;
    NVIC_IPR8(USART2_IRQ) = 0x80u;
    NVIC_ISER1 = (1u << (USART2_IRQ - 32u));
}

void ninebot_link_irq(void) {
    uint32_t sr = USART_SR_REG(USART2_BASE);
    if (sr & (USART_SR_RXNE | USART_SR_ORE)) {
        uint8_t b = (uint8_t)USART_DR_REG(USART2_BASE);
        if (sr & USART_SR_RXNE) {
            /* Stock-compatible staged IAP consumes the same physical stream independently. */
            iap_update_feed(b);
            uint16_t next = (uint16_t)((rx_head + 1u) & RX_RING_MASK);
            if (next == rx_tail) rx_overflow_count++;
            else { rx_ring[rx_head] = b; rx_head = next; }
        }
    }
}

static int rx_get(void) {
    if (rx_tail == rx_head) return -1;
    uint8_t b = rx_ring[rx_tail];
    rx_tail = (uint16_t)((rx_tail + 1u) & RX_RING_MASK);
    return (int)b;
}

static uint16_t checksum(const uint8_t *raw, uint8_t total) {
    uint16_t sum = 0u;
    for (uint8_t i = 2u; i < (uint8_t)(total - 2u); i++) sum = (uint16_t)(sum + raw[i]);
    return (uint16_t)(sum ^ 0xFFFFu);
}

/*
 * G30 stock BLE plaintext uses Ninebot 5A A5 framing:
 *   5A A5 LEN SRC DST CMD ARG PAYLOAD... CK0 CK1
 * where LEN is payload bytes and total = LEN + 9.
 *
 * Early DeltaESC builds incorrectly assumed the M365 55 AA envelope.  v0.6.3
 * accepts both on PA2 for a conservative first hardware test, and replies in
 * the same envelope that was received.  The 5A A5 path is the G30 path proven
 * by the physical BLE report; 55 AA is compatibility-only until scoped.
 */
uint8_t ninebot_link_get_frame(ninebot_frame_t *out) {
    static uint8_t raw[80];
    static uint8_t pos;
    static uint8_t expected;
    static uint8_t fmt;
    int ch;

    while ((ch = rx_get()) >= 0) {
        uint8_t b = (uint8_t)ch;
        if (pos == 0u) {
            if (b == 0x5Au) { raw[0] = b; pos = 1u; fmt = NINEBOT_FMT_5AA5; }
            else if (b == 0x55u) { raw[0] = b; pos = 1u; fmt = NINEBOT_FMT_55AA; }
            continue;
        }
        if (pos == 1u) {
            uint8_t want = (fmt == NINEBOT_FMT_5AA5) ? 0xA5u : 0xAAu;
            if (b == want) raw[pos++] = b;
            else {
                pos = 0u; expected = 0u;
                if (b == 0x5Au) { raw[0]=b; pos=1u; fmt=NINEBOT_FMT_5AA5; }
                else if (b == 0x55u) { raw[0]=b; pos=1u; fmt=NINEBOT_FMT_55AA; }
            }
            continue;
        }

        raw[pos++] = b;
        if (pos == 3u) {
            uint8_t len = raw[2];
            if (fmt == NINEBOT_FMT_5AA5) {
                expected = (uint8_t)(len + 9u);
                if (expected < 9u || expected > sizeof(raw) || len > NINEBOT_MAX_PAYLOAD) { pos=0u; expected=0u; }
            } else {
                expected = (uint8_t)(len + 6u);
                if (len < 2u || expected > sizeof(raw) || (uint8_t)(len-2u) > NINEBOT_MAX_PAYLOAD) { pos=0u; expected=0u; }
            }
            continue;
        }

        if (expected != 0u && pos == expected) {
            uint16_t got = (uint16_t)raw[expected - 2u] | ((uint16_t)raw[expected - 1u] << 8);
            uint16_t want = checksum(raw, expected);
            if (got == want) {
                out->format = fmt;
                if (fmt == NINEBOT_FMT_5AA5) {
                    out->src = raw[3]; out->dst = raw[4]; out->cmd = raw[5]; out->arg = raw[6];
                    out->payload_len = raw[2];
                    for (uint8_t i=0u;i<out->payload_len;i++) out->payload[i]=raw[7u+i];
                } else {
                    out->src = 0x3Eu; out->dst = raw[3]; out->cmd = raw[4]; out->arg = raw[5];
                    out->payload_len = (uint8_t)(raw[2]-2u);
                    for (uint8_t i=0u;i<out->payload_len;i++) out->payload[i]=raw[6u+i];
                }
                last_format = fmt;
                pos = 0u; expected = 0u;
                return 1u;
            }
            pos = 0u; expected = 0u;
        }
    }
    return 0u;
}

static void send_raw(const uint8_t *raw, uint8_t total) {
    uint32_t cr1 = USART_CR1_REG(USART2_BASE);
    USART_CR1_REG(USART2_BASE) = cr1 & ~(USART_CR1_RXNEIE | USART_CR1_RE);
    USART_CR1_REG(USART2_BASE) |= USART_CR1_TE;
    for (uint8_t i=0u;i<total;i++) {
        while (!(USART_SR_REG(USART2_BASE) & USART_SR_TXE)) {}
        USART_DR_REG(USART2_BASE) = raw[i];
    }
    while (!(USART_SR_REG(USART2_BASE) & USART_SR_TC)) {}
    USART_CR1_REG(USART2_BASE) &= ~USART_CR1_TE;
    USART_CR1_REG(USART2_BASE) |= USART_CR1_RE | USART_CR1_RXNEIE;
}

static void finalize_and_send(uint8_t *raw, uint8_t total) {
    uint16_t c = checksum(raw, total);
    raw[total-2u]=(uint8_t)c; raw[total-1u]=(uint8_t)(c>>8);
    send_raw(raw,total);
}

void ninebot_send_app_response(const ninebot_frame_t *request, uint8_t response_cmd,
                               uint8_t arg, const uint8_t *payload, uint8_t payload_len) {
    if (payload_len > 32u) payload_len = 32u;
    uint8_t raw[48];
    if (request && request->format == NINEBOT_FMT_5AA5) {
        uint8_t total=(uint8_t)(payload_len+9u);
        raw[0]=0x5Au; raw[1]=0xA5u; raw[2]=payload_len;
        raw[3]=0x20u; raw[4]=request->src ? request->src : 0x3Eu;
        raw[5]=response_cmd; raw[6]=arg;
        for(uint8_t i=0u;i<payload_len;i++) raw[7u+i]=payload[i];
        finalize_and_send(raw,total);
    } else {
        /* Legacy compatibility with v0.6.0/v0.6.2 test envelope. */
        uint8_t total=(uint8_t)(payload_len+8u);
        raw[0]=0x55u; raw[1]=0xAAu; raw[2]=(uint8_t)(payload_len+2u);
        raw[3]=0x23u; raw[4]=(response_cmd==NINEBOT_READ_ACK)?NINEBOT_READ:response_cmd; raw[5]=arg;
        for(uint8_t i=0u;i<payload_len;i++) raw[6u+i]=payload[i];
        finalize_and_send(raw,total);
    }
}

uint32_t ninebot_link_rx_overflows(void) { return rx_overflow_count; }
uint8_t ninebot_link_last_format(void) { return last_format; }

void ninebot_link_discard_rx(void) { irq_disable(); rx_tail = rx_head; irq_enable(); }
