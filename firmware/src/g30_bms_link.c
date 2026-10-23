#include "g30_bms_link.h"
#include "stm32f103_min.h"

#define BMS_RX_SIZE 64u
#define BMS_RX_MASK (BMS_RX_SIZE-1u)
#define BMS_TXEIE (1u<<7)
#define BMS_RX_ERRORS 15u /* PE, FE, NE, ORE; SR then DR clears */
#define BMS_POLL_MS 500u
#define BMS_TX_TIMEOUT_MS 20u

static g30_bms_t battery;
static volatile uint8_t rx[BMS_RX_SIZE],head,tail;
static volatile uint32_t rx_ms[BMS_RX_SIZE];
static volatile uint8_t rx_lost;
static volatile uint16_t errors;
static uint8_t tx[10];
static volatile uint8_t tx_pos,tx_len;
static uint32_t last_poll_ms,tx_started_ms;

static void error(void) { if(errors!=65535u)++errors; }

void g30_bms_link_init(uint32_t now) {
    g30_bms_init(&battery);head=tail=rx_lost=0u;errors=0u;
    tx_pos=tx_len=0u;last_poll_ms=now-BMS_POLL_MS;
    RCC_APB2ENR|=(1u<<3)|1u;RCC_APB1ENR|=(1u<<18);
    REG32(0x40010004u)&=~(3u<<4); /* USART3 no-remap: PB10/PB11 */
    /* Factory dedicated BMS link, PB10 AF push-pull / PB11 floating RX.
     * Source: SmartESC frozen G30 BMS port; never touches PA2 USART2 IAP. */
    GPIO_CRH(GPIOB_BASE)=(GPIO_CRH(GPIOB_BASE)&~0xff00u)|0x4a00u;
    USART_CR1_REG(G30_BMS_USART3_BASE)=0u;
    USART_CR2_REG(G30_BMS_USART3_BASE)=0u;
    USART_CR3_REG(G30_BMS_USART3_BASE)=0u;
    USART_BRR_REG(G30_BMS_USART3_BASE)=278u; /* APB1 32 MHz / 115200 */
    USART_CR1_REG(G30_BMS_USART3_BASE)=USART_CR1_UE|USART_CR1_RE|
                                               USART_CR1_TE|USART_CR1_RXNEIE;
    NVIC_IPR8(G30_BMS_IRQ)=0xc0u; /* below ADC and USART2/IAP */
    NVIC_ISER1=(1u<<(G30_BMS_IRQ-32u));
}
void g30_bms_link_irq(uint32_t now) {
    const uint32_t sr=USART_SR_REG(G30_BMS_USART3_BASE);
    if(sr&(USART_SR_RXNE|BMS_RX_ERRORS)) {
        const uint8_t v=(uint8_t)USART_DR_REG(G30_BMS_USART3_BASE);
        if(sr&BMS_RX_ERRORS){error();rx_lost=1u;}
        else {
            uint8_t next=(uint8_t)((head+1u)&BMS_RX_MASK);
            if(next==tail){error();rx_lost=1u;}
            else {rx[head]=v;rx_ms[head]=now;head=next;}
        }
    }
    if((sr&USART_SR_TXE) &&
       (USART_CR1_REG(G30_BMS_USART3_BASE)&BMS_TXEIE)) {
        if(tx_pos<tx_len)USART_DR_REG(G30_BMS_USART3_BASE)=tx[tx_pos++];
        if(tx_pos>=tx_len)USART_CR1_REG(G30_BMS_USART3_BASE)&=~BMS_TXEIE;
    }
}
void g30_bms_link_poll(uint32_t now,uint8_t suspended) {
    if(suspended) {
        USART_CR1_REG(G30_BMS_USART3_BASE)&=~BMS_TXEIE;
        tx_len=tx_pos=0u;tail=head;rx_lost=0u;
        g30_bms_reset_parser(&battery);last_poll_ms=now;
        return;
    }
    if(rx_lost) {
        /* An overrun makes a partial packet untrustworthy. Flush in a
         * short IRQ-preserving section; do not clear the motor/IAP latch. */
        uint32_t prior_primask;
        __asm volatile("mrs %0, primask" : "=r"(prior_primask) :: "memory");
        irq_disable();tail=head;rx_lost=0u;
        if(!(prior_primask&1u))irq_enable();
        g30_bms_reset_parser(&battery);
    }
    /* Fixed budget: UART noise cannot monopolize the main/IAP loop. */
    for(unsigned budget=0;budget<BMS_RX_SIZE && tail!=head;budget++) {
        uint8_t pos=tail;
        uint8_t v=rx[pos];uint32_t at=rx_ms[pos];
        tail=(uint8_t)((pos+1u)&BMS_RX_MASK);
        g30_bms_feed(&battery,v,at);
    }
    if(tx_pos<tx_len && (uint32_t)(now-tx_started_ms)>BMS_TX_TIMEOUT_MS) {
        USART_CR1_REG(G30_BMS_USART3_BASE)&=~BMS_TXEIE;
        tx_pos=tx_len=0u;error();
    }
    if(tx_pos>=tx_len && (uint32_t)(now-last_poll_ms)>=BMS_POLL_MS) {
        /* Publish the fully written packet before enabling its TX IRQ. */
        tx_len=0u;tx_pos=0u;
        uint8_t n=g30_bms_read_soc_request(tx);
        tx_started_ms=now;last_poll_ms=now;tx_len=n;
        USART_CR1_REG(G30_BMS_USART3_BASE)|=BMS_TXEIE;
    }
}
uint8_t g30_bms_link_soc(uint32_t now) { return g30_bms_soc(&battery,now); }
void g30_bms_link_status(uint32_t now,uint8_t out[G30_BMS_STATUS_BYTES]) {
    if(!out)return;
    g30_bms_status_payload(&battery,now,out);
    uint16_t n=errors;out[14]=(uint8_t)n;out[15]=(uint8_t)(n>>8);
}
