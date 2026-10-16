#include "uart.h"
#include "stm32f103_min.h"
static volatile uint8_t rx_head,rx_tail,rx_overflow;
static uint8_t rx_buf[256];
static void gpio_cfg(uint32_t base,unsigned pin,uint32_t nib){volatile uint32_t*reg=(volatile uint32_t*)((pin<8)?base:base+4u);unsigned sh=(pin&7u)*4u;uint32_t v=*reg;*reg=(v&~(0xFu<<sh))|((nib&0xFu)<<sh);}
void uart1_init(void){rx_head=rx_tail=rx_overflow=0;RCC_APB2ENR|=(1u<<0)|(1u<<3)|(1u<<14);AFIO_MAPR|=AFIO_MAPR_USART1_REMAP;gpio_cfg(GPIOB_BASE,6,0xBu);gpio_cfg(GPIOB_BASE,7,0x4u);USART_BRR=0x45u;USART_CR1=USART_CR1_UE|USART_CR1_TE|USART_CR1_RE|USART_CR1_RXNEIE;NVIC_ISER1=1u<<(37u-32u);}
void USART1_IRQHandler(void){uint32_t sr=USART_SR;if(sr&(USART_SR_RXNE|USART_SR_ORE)){uint8_t b=(uint8_t)USART_DR;uint8_t n=(uint8_t)(rx_head+1u);if(n!=rx_tail){rx_buf[rx_head]=b;rx_head=n;}else rx_overflow=1u;}}
int uart1_getc(void){if(rx_tail==rx_head)return -1;uint8_t b=rx_buf[rx_tail];rx_tail=(uint8_t)(rx_tail+1u);return (int)b;}
void uart1_putc(uint8_t c){while(!(USART_SR&USART_SR_TXE)){}USART_DR=c;}
void uart1_write(const uint8_t*p,uint32_t n){while(n--)uart1_putc(*p++);}
void uart1_wait_complete(void){while(!(USART_SR&USART_SR_TC)){} }
