#pragma once
#include <stdint.h>

#define REG32(a) (*(volatile uint32_t *)(a))
#define REG16(a) (*(volatile uint16_t *)(a))

#define RCC_BASE        0x40021000u
#define RCC_CR          REG32(RCC_BASE + 0x00)
#define RCC_CFGR        REG32(RCC_BASE + 0x04)
#define RCC_APB2ENR     REG32(RCC_BASE + 0x18)
#define RCC_APB1ENR     REG32(RCC_BASE + 0x1C)

#define FLASH_BASE      0x40022000u
#define FLASH_ACR       REG32(FLASH_BASE + 0x00)
#define FLASH_KEYR      REG32(FLASH_BASE + 0x04)
#define FLASH_SR        REG32(FLASH_BASE + 0x0C)
#define FLASH_CR        REG32(FLASH_BASE + 0x10)

#define GPIOA_BASE      0x40010800u
#define GPIOB_BASE      0x40010C00u
#define GPIOC_BASE      0x40011000u
#define GPIO_CRL(b)     REG32((b) + 0x00)
#define GPIO_CRH(b)     REG32((b) + 0x04)
#define GPIO_IDR(b)     REG32((b) + 0x08)
#define GPIO_ODR(b)     REG32((b) + 0x0C)
#define GPIO_BSRR(b)    REG32((b) + 0x10)
#define GPIO_BRR(b)     REG32((b) + 0x14)

#define TIM1_BASE       0x40012C00u
#define TIM_CR1(b)      REG32((b) + 0x00)
#define TIM_CR2(b)      REG32((b) + 0x04)
#define TIM_SMCR(b)     REG32((b) + 0x08)
#define TIM_DIER(b)     REG32((b) + 0x0C)
#define TIM_SR(b)       REG32((b) + 0x10)
#define TIM_EGR(b)      REG32((b) + 0x14)
#define TIM_CCMR1(b)    REG32((b) + 0x18)
#define TIM_CCMR2(b)    REG32((b) + 0x1C)
#define TIM_CCER(b)     REG32((b) + 0x20)
#define TIM_CNT(b)      REG32((b) + 0x24)
#define TIM_PSC(b)      REG32((b) + 0x28)
#define TIM_ARR(b)      REG32((b) + 0x2C)
#define TIM_RCR(b)      REG32((b) + 0x30)
#define TIM_CCR1(b)     REG32((b) + 0x34)
#define TIM_CCR2(b)     REG32((b) + 0x38)
#define TIM_CCR3(b)     REG32((b) + 0x3C)
#define TIM_CCR4(b)     REG32((b) + 0x40)
#define TIM_BDTR(b)     REG32((b) + 0x44)

#define ADC1_BASE       0x40012400u
#define ADC_SR          REG32(ADC1_BASE + 0x00)
#define ADC_CR1         REG32(ADC1_BASE + 0x04)
#define ADC_CR2         REG32(ADC1_BASE + 0x08)
#define ADC_SMPR1       REG32(ADC1_BASE + 0x0C)
#define ADC_SMPR2       REG32(ADC1_BASE + 0x10)
#define ADC_SQR1        REG32(ADC1_BASE + 0x2C)
#define ADC_SQR2        REG32(ADC1_BASE + 0x30)
#define ADC_SQR3        REG32(ADC1_BASE + 0x34)
#define ADC_JSQR        REG32(ADC1_BASE + 0x38)
#define ADC_JDR1        REG32(ADC1_BASE + 0x3C)
#define ADC_JDR2        REG32(ADC1_BASE + 0x40)
#define ADC_JDR3        REG32(ADC1_BASE + 0x44)
#define ADC_JDR4        REG32(ADC1_BASE + 0x48)
#define ADC_DR          REG32(ADC1_BASE + 0x4C)

#define USART1_BASE     0x40013800u
#define USART_SR        REG32(USART1_BASE + 0x00)
#define USART_DR        REG32(USART1_BASE + 0x04)
#define USART_BRR       REG32(USART1_BASE + 0x08)
#define USART_CR1       REG32(USART1_BASE + 0x0C)
#define USART_CR2       REG32(USART1_BASE + 0x10)
#define USART_CR3       REG32(USART1_BASE + 0x14)

#define USART2_BASE     0x40004400u
#define USART2_SR       REG32(USART2_BASE + 0x00)
#define USART2_DR       REG32(USART2_BASE + 0x04)
#define USART2_BRR      REG32(USART2_BASE + 0x08)
#define USART2_CR1      REG32(USART2_BASE + 0x0C)
#define USART2_CR2      REG32(USART2_BASE + 0x10)
#define USART2_CR3      REG32(USART2_BASE + 0x14)

#define SYST_CSR        REG32(0xE000E010u)
#define SYST_RVR        REG32(0xE000E014u)
#define SYST_CVR        REG32(0xE000E018u)
#define NVIC_ISER0      REG32(0xE000E100u)
#define NVIC_IPR_BASE   0xE000E400u
#define NVIC_IPR8(n)    (*(volatile uint8_t *)(NVIC_IPR_BASE + (n)))

#define COREDEBUG_DEMCR REG32(0xE000EDFCu)
#define DWT_CTRL        REG32(0xE0001000u)
#define DWT_CYCCNT      REG32(0xE0001004u)

#define TIM_CR1_CEN     (1u << 0)
#define TIM_CR1_CMS_0   (1u << 5)
#define TIM_CR1_ARPE    (1u << 7)
#define TIM_EGR_UG      (1u << 0)

#define TIM_CCER_CC1E   (1u << 0)
#define TIM_CCER_CC1NE  (1u << 2)
#define TIM_CCER_CC2E   (1u << 4)
#define TIM_CCER_CC2NE  (1u << 6)
#define TIM_CCER_CC3E   (1u << 8)
#define TIM_CCER_CC3NE  (1u << 10)
#define TIM_CCER_CC4E   (1u << 12)

#define TIM_BDTR_OSSI   (1u << 10)
#define TIM_BDTR_OSSR   (1u << 11)
#define TIM_BDTR_MOE    (1u << 15)

#define ADC_SR_JEOC      (1u << 2)
#define ADC_CR1_JEOCIE   (1u << 7)
#define ADC_CR1_SCAN     (1u << 8)
#define ADC_CR2_ADON     (1u << 0)
#define ADC_CR2_CAL      (1u << 2)
#define ADC_CR2_RSTCAL   (1u << 3)
#define ADC_CR2_JEXTSEL_MASK (7u << 12)
#define ADC_CR2_JEXTSEL_TIM1_CC4 (1u << 12)
#define ADC_CR2_JEXTSEL_JSWSTART (7u << 12)
#define ADC_CR2_JEXTTRIG (1u << 15)
#define ADC_CR2_JSWSTART (1u << 21)

#define USART_SR_ORE    (1u << 3)
#define USART_SR_RXNE   (1u << 5)
#define USART_SR_TC     (1u << 6)
#define USART_SR_TXE    (1u << 7)
#define USART_CR1_RE    (1u << 2)
#define USART_CR1_TE    (1u << 3)
#define USART_CR1_UE    (1u << 13)
#define USART_CR3_HDSEL (1u << 3)

static inline void irq_disable(void) { __asm volatile("cpsid i" ::: "memory"); }
static inline void irq_enable(void)  { __asm volatile("cpsie i" ::: "memory"); }
static inline void nop(void)         { __asm volatile("nop"); }
