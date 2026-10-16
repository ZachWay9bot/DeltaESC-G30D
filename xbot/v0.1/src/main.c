#include <stdint.h>
#include "stm32f103_min.h"
#include "bemf_detector.h"

#define SYSCLK_HZ 64000000u
#define POLE_PAIRS_DEFAULT 15u
#define BEMF_THRESHOLD_SPAN 24u
#define SAMPLE_PERIOD_MS 1u
#define REPORT_PERIOD_MS 100u

volatile uint32_t g_ms;
volatile uint32_t g_fault;
static uint16_t adc_vbat,adc_ia,adc_ib,adc_ic,adc_bemf_a,adc_bemf_b,adc_bemf_c;
static bemf_detector_t bemf;
static uint8_t continuous;

extern uint32_t _estack,_sidata,_sdata,_edata,_sbss,_ebss;
int main(void);
void Reset_Handler(void);
void Default_Handler(void);
void SysTick_Handler(void);

__attribute__((section(".isr_vector"),used))
void (* const vectors[])(void)={
 (void(*)(void))(&_estack),Reset_Handler,Default_Handler,Default_Handler,
 Default_Handler,Default_Handler,Default_Handler,Default_Handler,
 Default_Handler,Default_Handler,Default_Handler,Default_Handler,
 Default_Handler,Default_Handler,Default_Handler,SysTick_Handler,
 Default_Handler,Default_Handler,Default_Handler,Default_Handler,
 Default_Handler,Default_Handler,Default_Handler,Default_Handler,
 Default_Handler,Default_Handler,Default_Handler,Default_Handler,
 Default_Handler,Default_Handler,Default_Handler,Default_Handler,
 Default_Handler,Default_Handler,Default_Handler,Default_Handler,
 Default_Handler,Default_Handler,Default_Handler,Default_Handler,
 Default_Handler,Default_Handler,Default_Handler,Default_Handler,
 Default_Handler,Default_Handler,Default_Handler,Default_Handler,
 Default_Handler,Default_Handler,Default_Handler,Default_Handler,
 Default_Handler,Default_Handler,Default_Handler,Default_Handler,
 Default_Handler,Default_Handler,Default_Handler,Default_Handler
};

static void gpio_cfg_nibble(uint32_t base,unsigned pin,uint32_t nibble){
 volatile uint32_t *reg=(volatile uint32_t *)((pin<8)?(base+0x00u):(base+0x04u));
 unsigned shift=(pin&7u)*4u;
 uint32_t v=*reg;
 *reg=(v&~(0xFu<<shift))|((nibble&0xFu)<<shift);
}

static void clock_64mhz_hsi(void){
 RCC_CR|=1u<<0; while(!(RCC_CR&(1u<<1))){}
 FLASH_ACR=(1u<<4)|2u;
 uint32_t cfgr=0u;
 cfgr|=(4u<<8);
 cfgr|=(2u<<14);
 cfgr|=(14u<<18);
 RCC_CFGR=cfgr;
 RCC_CR|=1u<<24; while(!(RCC_CR&(1u<<25))){}
 RCC_CFGR=(RCC_CFGR&~3u)|2u;
 while(((RCC_CFGR>>2)&3u)!=2u){}
}

static void dwt_init(void){COREDEBUG_DEMCR|=1u<<24;DWT_CYCCNT=0u;DWT_CTRL|=1u;}
static void systick_init(void){SYST_RVR=(SYSCLK_HZ/1000u)-1u;SYST_CVR=0u;SYST_CSR=7u;}

static void xbot_safe_gpio_init(void){
 RCC_APB2ENR|=(1u<<0)|(1u<<2)|(1u<<3);
 gpio_cfg_nibble(GPIOA_BASE,11,0x2u);
 GPIO_BSRR(GPIOA_BASE)=1u<<11;

 gpio_cfg_nibble(GPIOA_BASE,8,0x8u);
 gpio_cfg_nibble(GPIOA_BASE,9,0x8u);
 gpio_cfg_nibble(GPIOA_BASE,10,0x8u);
 gpio_cfg_nibble(GPIOB_BASE,13,0x8u);
 gpio_cfg_nibble(GPIOB_BASE,14,0x8u);
 gpio_cfg_nibble(GPIOB_BASE,15,0x8u);
 GPIO_BRR(GPIOA_BASE)=(1u<<8)|(1u<<9)|(1u<<10);
 GPIO_BRR(GPIOB_BASE)=(1u<<13)|(1u<<14)|(1u<<15);

 gpio_cfg_nibble(GPIOA_BASE,1,0x0u);
 gpio_cfg_nibble(GPIOA_BASE,3,0x0u);
 gpio_cfg_nibble(GPIOA_BASE,4,0x0u);
 gpio_cfg_nibble(GPIOA_BASE,5,0x0u);
 gpio_cfg_nibble(GPIOA_BASE,6,0x0u);
 gpio_cfg_nibble(GPIOA_BASE,7,0x0u);
 gpio_cfg_nibble(GPIOB_BASE,1,0x0u); /* Xbot BEMF_C = ADC9, never gate enable */

 RCC_APB2RSTR|=(1u<<11);
 RCC_APB2RSTR&=~(1u<<11);
}

static void uart1_init(void){
 RCC_APB2ENR|=(1u<<14)|(1u<<3);
 gpio_cfg_nibble(GPIOB_BASE,6,0xBu);
 gpio_cfg_nibble(GPIOB_BASE,7,0x4u);
 USART_BRR=556u;
 USART_CR1=USART_CR1_UE|USART_CR1_TE|USART_CR1_RE;
}
static void putc1(char c){while(!(USART_SR&USART_SR_TXE)){}USART_DR=(uint8_t)c;}
static void puts1(const char*s){while(*s)putc1(*s++);}
static void putu(uint32_t v){char b[11];unsigned n=0;if(!v){putc1('0');return;}while(v&&n<sizeof b){b[n++]=(char)('0'+v%10u);v/=10u;}while(n)putc1(b[--n]);}
static void puti(int32_t v){if(v<0){putc1('-');v=-v;}putu((uint32_t)v);}
static int getc1(void){if(!(USART_SR&USART_SR_RXNE))return -1;return (int)(USART_DR&0xFFu);}

static void adc_init(void){
 RCC_APB2ENR|=(1u<<9)|(1u<<2)|(1u<<3);
 ADC_CR1=0u;ADC_SMPR1=0u;ADC_SMPR2=0u;
 const unsigned chans[]={1u,3u,4u,5u,6u,7u,9u};
 for(unsigned i=0;i<7u;i++)ADC_SMPR2|=(4u<<(chans[i]*3u));
 ADC_CR2=ADC_CR2_ADON;
 for(volatile unsigned i=0;i<2000u;i++)nop();
 ADC_CR2|=ADC_CR2_RSTCAL;while(ADC_CR2&ADC_CR2_RSTCAL){}
 ADC_CR2|=ADC_CR2_CAL;while(ADC_CR2&ADC_CR2_CAL){}
 ADC_CR2=ADC_CR2_ADON|ADC_CR2_EXTSEL_SWSTART|ADC_CR2_EXTTRIG;
}
static uint16_t adc_read(unsigned ch){
 ADC_SQR1=0u;ADC_SQR2=0u;ADC_SQR3=ch&0x1Fu;
 ADC_SR&=~ADC_SR_EOC;
 ADC_CR2|=ADC_CR2_SWSTART;
 uint32_t start=DWT_CYCCNT;
 while(!(ADC_SR&ADC_SR_EOC)){
  if((uint32_t)(DWT_CYCCNT-start)>64000u){g_fault=0xADC00001u;return 0xFFFFu;}
 }
 return (uint16_t)ADC_DR;
}
static void sample_all(void){
 adc_vbat=adc_read(1u);
 adc_ia=adc_read(3u);adc_ib=adc_read(4u);adc_ic=adc_read(5u);
 adc_bemf_a=adc_read(6u);adc_bemf_b=adc_read(7u);adc_bemf_c=adc_read(9u);
 if(!g_fault)bemf_detector_update(&bemf,adc_bemf_a,adc_bemf_b,adc_bemf_c,DWT_CYCCNT);
}
static void report(void){
 puts1("XBOT-PROBE v0.1 SAFE fault=");putu(g_fault);
 puts1(" vbat=");putu(adc_vbat);
 puts1(" Iabc=");putu(adc_ia);putc1('/');putu(adc_ib);putc1('/');putu(adc_ic);
 puts1(" BEMF=");putu(adc_bemf_a);putc1('/');putu(adc_bemf_b);putc1('/');putu(adc_bemf_c);
 puts1(" cm=");putu(bemf.common);puts1(" span=");putu(bemf.span);puts1(" sec=");
 if(bemf.sector==BEMF_NO_SECTOR)puts1("-");else putu(bemf.sector);
 puts1(" dir=");puti(bemf.direction);
 puts1(" trans=");putu(bemf.transitions);puts1(" bad=");putu(bemf.bad_jumps);
 puts1(" eHzx10=");putu(bemf_electrical_hz_x10(&bemf,SYSCLK_HZ));
 puts1(" rpmx10=");putu(bemf_mechanical_rpm_x10(&bemf,SYSCLK_HZ,POLE_PAIRS_DEFAULT));
 puts1(" TIM1=");putu(TIM_CR1(TIM1_BASE));putc1('/');putu(TIM_CCER(TIM1_BASE));putc1('/');putu(TIM_BDTR(TIM1_BASE));
 puts1("\r\n");
}
static void help(void){
 puts1("commands: s=status c=continuous(10Hz) r=reset-stats ?=help\r\n");
 puts1("NO HALLS. NO PWM. NO MOE. NO TORQUE. PB1=BEMF_C analog.\r\n");
}

void Reset_Handler(void){
 uint32_t *src=&_sidata;
 for(uint32_t *dst=&_sdata;dst<&_edata;)*dst++=*src++;
 for(uint32_t *dst=&_sbss;dst<&_ebss;)*dst++=0u;
 (void)main();
 for(;;){}
}
void Default_Handler(void){g_fault=0xDEAD0001u;for(;;){}}
void SysTick_Handler(void){g_ms++;}

int main(void){
 irq_disable();
 SCB_VTOR=0x08001000u;
 clock_64mhz_hsi();
 dwt_init();
 xbot_safe_gpio_init();
 uart1_init();
 adc_init();
 systick_init();
 bemf_detector_init(&bemf,BEMF_THRESHOLD_SPAN);
 irq_enable();

 puts1("\r\nXBOT SENSORLESS PASSIVE BEMF PROBE v0.1\r\n");
 help();
 uint32_t last_sample=0u,last_report=0u;
 for(;;){
  uint32_t now=g_ms;
  if((uint32_t)(now-last_sample)>=SAMPLE_PERIOD_MS){last_sample=now;sample_all();}
  int c=getc1();
  if(c=='s'||c=='S')report();
  else if(c=='c'||c=='C'){continuous^=1u;puts1(continuous?"continuous=1\r\n":"continuous=0\r\n");}
  else if(c=='r'||c=='R'){bemf_detector_reset_stats(&bemf);puts1("stats reset\r\n");}
  else if(c=='?')help();
  if(continuous&&(uint32_t)(now-last_report)>=REPORT_PERIOD_MS){last_report=now;report();}
 }
}
