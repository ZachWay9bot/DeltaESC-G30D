#include <stdint.h>
#include "stm32f103_min.h"
#include "uart.h"
#include "protocol.h"
#include "iap.h"
#include "bemf_detector.h"

#define SYSCLK_HZ 8000000u
#define POLE_PAIRS_DEFAULT 15u
#define BEMF_THRESHOLD_SPAN 24u

volatile uint32_t g_ms;
volatile uint32_t g_fault;
static uint16_t adc_vbat,adc_ia,adc_ib,adc_ic,adc_bemf_a,adc_bemf_b,adc_bemf_c;
static bemf_detector_t bemf;
extern uint32_t _estack,_sidata,_sdata,_edata,_sbss,_ebss;
int main(void);void Reset_Handler(void);void Default_Handler(void);void SysTick_Handler(void);void USART1_IRQHandler(void);

__attribute__((section(".isr_vector"),used))
void (* const vectors[59])(void)={
 (void(*)(void))(&_estack),
 Reset_Handler,
 Default_Handler,
 Default_Handler,
 Default_Handler,
 Default_Handler,
 Default_Handler,
 Default_Handler,
 Default_Handler,
 Default_Handler,
 Default_Handler,
 Default_Handler,
 Default_Handler,
 Default_Handler,
 Default_Handler,
 SysTick_Handler,
 Default_Handler,
 Default_Handler,
 Default_Handler,
 Default_Handler,
 Default_Handler,
 Default_Handler,
 Default_Handler,
 Default_Handler,
 Default_Handler,
 Default_Handler,
 Default_Handler,
 Default_Handler,
 Default_Handler,
 Default_Handler,
 Default_Handler,
 Default_Handler,
 Default_Handler,
 Default_Handler,
 Default_Handler,
 Default_Handler,
 Default_Handler,
 Default_Handler,
 Default_Handler,
 Default_Handler,
 Default_Handler,
 Default_Handler,
 Default_Handler,
 Default_Handler,
 Default_Handler,
 Default_Handler,
 Default_Handler,
 Default_Handler,
 Default_Handler,
 Default_Handler,
 Default_Handler,
 Default_Handler,
 Default_Handler,
 USART1_IRQHandler,
 Default_Handler,
 Default_Handler,
 Default_Handler,
 Default_Handler,
 Default_Handler
};

static void gpio_cfg(uint32_t base,unsigned pin,uint32_t nib){volatile uint32_t*reg=(volatile uint32_t*)((pin<8)?base:base+4u);unsigned sh=(pin&7u)*4u;uint32_t v=*reg;*reg=(v&~(0xFu<<sh))|((nib&0xFu)<<sh);}
static void clock_hsi8_clean(void){RCC_CR|=1u;while(!(RCC_CR&(1u<<1))){}RCC_CFGR&=~3u;while(((RCC_CFGR>>2)&3u)!=0u){}RCC_CR&=~((1u<<24)|(1u<<19)|(1u<<16));RCC_CFGR=0u;FLASH_ACR=0u;}
static void dwt_init(void){COREDEBUG_DEMCR|=1u<<24;DWT_CYCCNT=0;DWT_CTRL|=1u;}
static void systick_init(void){SYST_RVR=(SYSCLK_HZ/1000u)-1u;SYST_CVR=0;SYST_CSR=7u;}

static void safe_gpio_init(void){
 RCC_APB2ENR|=(1u<<0)|(1u<<2)|(1u<<3);
 /* Power hold: Xbot schematic PA11=PWR_EN. */
 gpio_cfg(GPIOA_BASE,11,0x2u);GPIO_BSRR(GPIOA_BASE)=1u<<11;
 /* Six bridge command pins stay input/pulldown; TIM1 is reset and not clocked. */
 gpio_cfg(GPIOA_BASE,8,0x8u);gpio_cfg(GPIOA_BASE,9,0x8u);gpio_cfg(GPIOA_BASE,10,0x8u);
 gpio_cfg(GPIOB_BASE,13,0x8u);gpio_cfg(GPIOB_BASE,14,0x8u);gpio_cfg(GPIOB_BASE,15,0x8u);
 GPIO_BRR(GPIOA_BASE)=(1u<<8)|(1u<<9)|(1u<<10);GPIO_BRR(GPIOB_BASE)=(1u<<13)|(1u<<14)|(1u<<15);
 /* Analog inputs from supplied Xbot schematic. PB1 is BEMF_C/ADC9. */
 gpio_cfg(GPIOA_BASE,1,0x0u);gpio_cfg(GPIOA_BASE,3,0x0u);gpio_cfg(GPIOA_BASE,4,0x0u);gpio_cfg(GPIOA_BASE,5,0x0u);
 gpio_cfg(GPIOA_BASE,6,0x0u);gpio_cfg(GPIOA_BASE,7,0x0u);gpio_cfg(GPIOB_BASE,1,0x0u);
 RCC_APB2RSTR|=1u<<11;RCC_APB2RSTR&=~(1u<<11);RCC_APB2ENR&=~(1u<<11);
}

static void adc_init(void){RCC_APB2ENR|=(1u<<9)|(1u<<2)|(1u<<3);ADC_CR1=0;ADC_SMPR1=0;ADC_SMPR2=0;const unsigned ch[]={1,3,4,5,6,7,9};for(unsigned i=0;i<7;i++)ADC_SMPR2|=4u<<(ch[i]*3u);ADC_CR2=ADC_CR2_ADON;for(volatile unsigned i=0;i<3000;i++)nop();ADC_CR2|=ADC_CR2_RSTCAL;while(ADC_CR2&ADC_CR2_RSTCAL){}ADC_CR2|=ADC_CR2_CAL;while(ADC_CR2&ADC_CR2_CAL){}ADC_CR2=ADC_CR2_ADON|ADC_CR2_EXTSEL_SWSTART|ADC_CR2_EXTTRIG;}
static uint16_t adc_read(unsigned ch){ADC_SQR1=0;ADC_SQR2=0;ADC_SQR3=ch&31u;ADC_SR&=~ADC_SR_EOC;ADC_CR2|=ADC_CR2_SWSTART;uint32_t st=DWT_CYCCNT;while(!(ADC_SR&ADC_SR_EOC)){if((uint32_t)(DWT_CYCCNT-st)>80000u){g_fault=0xADC00001u;return 0xffffu;}}return (uint16_t)ADC_DR;}
static void sample_all(void){adc_vbat=adc_read(1);adc_ia=adc_read(3);adc_ib=adc_read(4);adc_ic=adc_read(5);adc_bemf_a=adc_read(6);adc_bemf_b=adc_read(7);adc_bemf_c=adc_read(9);if(!g_fault){uint32_t now=DWT_CYCCNT;bemf_detector_update(&bemf,adc_bemf_a,adc_bemf_b,adc_bemf_c,now);bemf_detector_expire(&bemf,now,SYSCLK_HZ/4u);}}
static void wr16(uint8_t*p,uint16_t v){p[0]=(uint8_t)v;p[1]=(uint8_t)(v>>8);}static void wr32(uint8_t*p,uint32_t v){p[0]=(uint8_t)v;p[1]=(uint8_t)(v>>8);p[2]=(uint8_t)(v>>16);p[3]=(uint8_t)(v>>24);}

static uint16_t map_word(uint8_t idx){
 static const uint8_t sn[14]={'X','B','O','T','S','L','0','2','V','0','0','2','0','1'};
 if(idx>=16u&&idx<23u){unsigned o=(idx-16u)*2u;return (uint16_t)sn[o]|((uint16_t)sn[o+1]<<8);}
 if(idx==26u)return 0x0201u;
 if(idx>=218u&&idx<224u){const uint8_t*uid=(const uint8_t*)0x1FFFF7E8u;unsigned o=(idx-218u)*2u;return (uint16_t)uid[o]|((uint16_t)uid[o+1]<<8);}
 return 0u;
}
static void cmd_read_map(const nb_frame_t*f){if(f->len!=1u){return;}uint8_t n=f->data[0];if(n>NB_MAX_DATA)n=NB_MAX_DATA;uint8_t out[NB_MAX_DATA];for(uint8_t i=0;i<n;i++){uint16_t w=map_word((uint8_t)(f->index+(i>>1)));out[i]=(uint8_t)(w>>((i&1u)*8u));}nb_send(f->src,0x04u,f->index,out,n);}
static void cmd_diag(const nb_frame_t*f){uint8_t d[44];d[0]='X';d[1]='B';d[2]='0';d[3]='2';wr32(d+4,g_ms);wr16(d+8,adc_vbat);wr16(d+10,adc_ia);wr16(d+12,adc_ib);wr16(d+14,adc_ic);wr16(d+16,adc_bemf_a);wr16(d+18,adc_bemf_b);wr16(d+20,adc_bemf_c);wr16(d+22,bemf.common);wr16(d+24,bemf.span);d[26]=bemf.sector;d[27]=(uint8_t)bemf.direction;wr32(d+28,bemf.transitions);wr32(d+32,bemf.bad_jumps);wr32(d+36,bemf_electrical_hz_x10(&bemf,SYSCLK_HZ));wr32(d+40,bemf_mechanical_rpm_x10(&bemf,SYSCLK_HZ,POLE_PAIRS_DEFAULT));nb_send(f->src,0xF0u,0,d,sizeof d);}

static void dispatch(const nb_frame_t*f){
 if(f->dst!=0x20u)return;
 if(f->cmd==0x01u){cmd_read_map(f);return;}
 if(f->cmd==0x07u){nb_ack(f->src,iap_begin(f->data,f->len));return;}
 if(f->cmd==0x08u){nb_ack(f->src,iap_write(f->index,f->data,f->len));return;}
 if(f->cmd==0x09u){nb_ack(f->src,iap_finalize(f->data,f->len));return;}
 if(f->cmd==0x0Au){nb_ack(f->src,IAP_OK);uart1_wait_complete();for(volatile uint32_t i=0;i<80000u;i++)nop();SCB_AIRCR=SCB_AIRCR_VECTKEY|SCB_AIRCR_SYSRESETREQ;for(;;){} }
 if(f->cmd==0xF0u){cmd_diag(f);return;}
 if(f->cmd==0xF1u){bemf_detector_reset_stats(&bemf);nb_ack(f->src,IAP_OK);return;}
}

void Reset_Handler(void){uint32_t*src=&_sidata;for(uint32_t*dst=&_sdata;dst<&_edata;)*dst++=*src++;for(uint32_t*dst=&_sbss;dst<&_ebss;)*dst++=0;(void)main();for(;;){}}
void Default_Handler(void){g_fault=0xDEAD0001u;for(;;){}}
void SysTick_Handler(void){g_ms++;}
int main(void){irq_disable();SCB_VTOR=APP_BASE;clock_hsi8_clean();dwt_init();safe_gpio_init();uart1_init();adc_init();systick_init();bemf_detector_init(&bemf,BEMF_THRESHOLD_SPAN);iap_init();nb_parser_init();irq_enable();uint32_t last=0;nb_frame_t f;for(;;){uint32_t now=g_ms;if((uint32_t)(now-last)>=1u){last=now;sample_all();}int c=uart1_getc();if(c>=0&&nb_parser_push((uint8_t)c,&f))dispatch(&f);}}
