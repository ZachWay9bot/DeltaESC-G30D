#pragma once
#include <stdint.h>
#define REG32(a) (*(volatile uint32_t *)(a))
#define RCC_BASE 0x40021000u
#define RCC_CR REG32(RCC_BASE+0x00)
#define RCC_CFGR REG32(RCC_BASE+0x04)
#define RCC_APB2RSTR REG32(RCC_BASE+0x0C)
#define RCC_APB2ENR REG32(RCC_BASE+0x18)
#define FLASH_ACR REG32(0x40022000u)
#define GPIOA_BASE 0x40010800u
#define GPIOB_BASE 0x40010C00u
#define GPIO_CRL(b) REG32((b)+0x00)
#define GPIO_CRH(b) REG32((b)+0x04)
#define GPIO_IDR(b) REG32((b)+0x08)
#define GPIO_ODR(b) REG32((b)+0x0C)
#define GPIO_BSRR(b) REG32((b)+0x10)
#define GPIO_BRR(b) REG32((b)+0x14)
#define TIM1_BASE 0x40012C00u
#define TIM_CR1(b) REG32((b)+0x00)
#define TIM_CCER(b) REG32((b)+0x20)
#define TIM_BDTR(b) REG32((b)+0x44)
#define ADC1_BASE 0x40012400u
#define ADC_SR REG32(ADC1_BASE+0x00)
#define ADC_CR1 REG32(ADC1_BASE+0x04)
#define ADC_CR2 REG32(ADC1_BASE+0x08)
#define ADC_SMPR1 REG32(ADC1_BASE+0x0C)
#define ADC_SMPR2 REG32(ADC1_BASE+0x10)
#define ADC_SQR1 REG32(ADC1_BASE+0x2C)
#define ADC_SQR2 REG32(ADC1_BASE+0x30)
#define ADC_SQR3 REG32(ADC1_BASE+0x34)
#define ADC_DR REG32(ADC1_BASE+0x4C)
#define ADC_SR_EOC (1u<<1)
#define ADC_CR2_ADON (1u<<0)
#define ADC_CR2_CAL (1u<<2)
#define ADC_CR2_RSTCAL (1u<<3)
#define ADC_CR2_EXTSEL_SWSTART (7u<<17)
#define ADC_CR2_EXTTRIG (1u<<20)
#define ADC_CR2_SWSTART (1u<<22)
#define USART1_BASE 0x40013800u
#define USART_SR REG32(USART1_BASE+0x00)
#define USART_DR REG32(USART1_BASE+0x04)
#define USART_BRR REG32(USART1_BASE+0x08)
#define USART_CR1 REG32(USART1_BASE+0x0C)
#define USART_SR_RXNE (1u<<5)
#define USART_SR_TXE (1u<<7)
#define USART_CR1_RE (1u<<2)
#define USART_CR1_TE (1u<<3)
#define USART_CR1_UE (1u<<13)
#define SYST_CSR REG32(0xE000E010u)
#define SYST_RVR REG32(0xE000E014u)
#define SYST_CVR REG32(0xE000E018u)
#define SCB_VTOR REG32(0xE000ED08u)
#define COREDEBUG_DEMCR REG32(0xE000EDFCu)
#define DWT_CTRL REG32(0xE0001000u)
#define DWT_CYCCNT REG32(0xE0001004u)
static inline void irq_disable(void){__asm volatile("cpsid i":::"memory");}
static inline void irq_enable(void){__asm volatile("cpsie i":::"memory");}
static inline void nop(void){__asm volatile("nop");}
