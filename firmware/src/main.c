#include <stdint.h>
#include "stm32f103_min.h"
#include "control_diag.h"

#ifndef POWER_STAGE_ARM_ALLOWED
#define POWER_STAGE_ARM_ALLOWED 0
#endif

#define SYSCLK_HZ                  64000000u
#define PWM_HZ                     16000u
#define CONTROL_HZ                  4000u
#define PWM_ARR                     1999u
#define PWM_ZERO_CCR               ((PWM_ARR + 1u) / 2u)
#define ADC_TRIGGER_CCR             1800u
#define CONTROL_BUDGET_CYCLES      (SYSCLK_HZ / CONTROL_HZ)
#define CONTROL_WARN_CYCLES         12000u
#define CONTROL_HARD_CYCLES         15000u
#define PWM_SAMPLE_EXPECT_CYCLES    (SYSCLK_HZ / PWM_HZ)
#define DEADTIME_DTG                  64u
#define ARM_BOOT_DELAY_MS            3000u
#define ARM_MIN_ADC_SAMPLES          2000u
#define ARM_IDLE_CURRENT_COUNTS       120u
#define HARD_OC_COUNTS                700u
#define ADC_STALE_MS                    5u

#define IRQ_ADC1_2 18u

#define GATE_CCER_MASK (TIM_CCER_CC1E | TIM_CCER_CC1NE |                         TIM_CCER_CC2E | TIM_CCER_CC2NE |                         TIM_CCER_CC3E | TIM_CCER_CC3NE)
#define ADC_CCER_MASK TIM_CCER_CC4E

volatile uint32_t g_ms;
volatile uint32_t g_adc_samples;
volatile uint32_t g_control_ticks;
volatile uint32_t g_control_hard_overruns;
volatile uint32_t g_safety_latched;
volatile uint32_t g_overcurrent_trips;
volatile uint32_t g_adc_last_ms;
volatile uint32_t g_last_sample_cycle;
volatile uint32_t g_sample_interval_min = 0xFFFFFFFFu;
volatile uint32_t g_sample_interval_max;
volatile uint32_t g_last_isr_cycles;
volatile uint32_t g_max_isr_cycles;
volatile uint32_t g_last_control_cycles;
volatile uint32_t g_max_control_cycles;
volatile uint16_t g_adc_raw[4];
volatile uint16_t g_adc_offset[3];
volatile uint16_t g_last_abs_current;
volatile uint16_t g_peak_abs_current;
volatile uint8_t  g_power_armed;
volatile uint8_t  g_control_div4;

static control_diag_state_t ctrl;

extern uint32_t _estack, _sidata, _sdata, _edata, _sbss, _ebss;
int main(void);
void Reset_Handler(void);
void Default_Handler(void);
void SysTick_Handler(void);
void ADC1_2_IRQHandler(void);

__attribute__((section(".isr_vector"), used))
void (* const vectors[])(void) = {
    (void (*)(void))(&_estack), Reset_Handler, Default_Handler, Default_Handler,
    Default_Handler, Default_Handler, Default_Handler, Default_Handler,
    Default_Handler, Default_Handler, Default_Handler, Default_Handler,
    Default_Handler, Default_Handler, Default_Handler, SysTick_Handler,
    Default_Handler, Default_Handler, Default_Handler, Default_Handler,
    Default_Handler, Default_Handler, Default_Handler, Default_Handler,
    Default_Handler, Default_Handler, Default_Handler, Default_Handler,
    Default_Handler, Default_Handler, Default_Handler, Default_Handler,
    Default_Handler, Default_Handler,
    ADC1_2_IRQHandler,
    Default_Handler, Default_Handler, Default_Handler, Default_Handler,
    Default_Handler, Default_Handler, Default_Handler, Default_Handler,
    Default_Handler, Default_Handler, Default_Handler, Default_Handler,
    Default_Handler, Default_Handler, Default_Handler, Default_Handler,
    Default_Handler, Default_Handler, Default_Handler, Default_Handler,
    Default_Handler, Default_Handler, Default_Handler, Default_Handler,
    Default_Handler
};

static void gpio_cfg_nibble(uint32_t base, unsigned pin, uint32_t nibble) {
    volatile uint32_t *reg = (volatile uint32_t *)((pin < 8) ? (base + 0x00u) : (base + 0x04u));
    unsigned shift = (pin & 7u) * 4u;
    uint32_t v = *reg;
    v = (v & ~(0xFu << shift)) | ((nibble & 0xFu) << shift);
    *reg = v;
}

static uint32_t gate_pin_mode_word_a(void) {
    return (GPIO_CRH(GPIOA_BASE) >> 0) & 0xFFFu;
}

static uint32_t gate_pin_mode_word_b(void) {
    return (GPIO_CRH(GPIOB_BASE) >> 20) & 0xFFFu;
}

static void clock_64mhz_hsi(void) {
    RCC_CR |= (1u << 0);
    while (!(RCC_CR & (1u << 1))) {}
    FLASH_ACR = (1u << 4) | 2u;
    uint32_t cfgr = 0;
    cfgr |= (4u << 8);
    cfgr |= (2u << 14);
    cfgr |= (14u << 18);
    RCC_CFGR = cfgr;
    RCC_CR |= (1u << 24);
    while (!(RCC_CR & (1u << 25))) {}
    RCC_CFGR = (RCC_CFGR & ~3u) | 2u;
    while (((RCC_CFGR >> 2) & 3u) != 2u) {}
}

static void dwt_init(void) {
    COREDEBUG_DEMCR |= (1u << 24);
    DWT_CYCCNT = 0;
    DWT_CTRL |= 1u;
}

static void systick_init(void) {
    SYST_RVR = (SYSCLK_HZ / 1000u) - 1u;
    SYST_CVR = 0;
    SYST_CSR = 7u;
}

#if POWER_STAGE_ARM_ALLOWED
static void delay_cycles(uint32_t cycles) {
    uint32_t s = DWT_CYCCNT;
    while ((uint32_t)(DWT_CYCCNT - s) < cycles) {}
}
#endif

static void power_hold_init(void) {
    RCC_APB2ENR |= (1u << 0) | (1u << 2) | (1u << 3) | (1u << 4);
    gpio_cfg_nibble(GPIOA_BASE, 11, 0x2);
    GPIO_BSRR(GPIOA_BASE) = (1u << 11);
}

static void gate_pins_to_safe_inputs(void) {
    gpio_cfg_nibble(GPIOA_BASE, 8, 0x8);
    gpio_cfg_nibble(GPIOA_BASE, 9, 0x8);
    gpio_cfg_nibble(GPIOA_BASE,10, 0x8);
    gpio_cfg_nibble(GPIOB_BASE,13, 0x8);
    gpio_cfg_nibble(GPIOB_BASE,14, 0x8);
    gpio_cfg_nibble(GPIOB_BASE,15, 0x8);
    GPIO_BRR(GPIOA_BASE) = (1u<<8)|(1u<<9)|(1u<<10);
    GPIO_BRR(GPIOB_BASE) = (1u<<13)|(1u<<14)|(1u<<15);
}

#if POWER_STAGE_ARM_ALLOWED
static void gate_pins_to_tim1_af(void) {
    gpio_cfg_nibble(GPIOA_BASE, 8, 0xB);
    gpio_cfg_nibble(GPIOA_BASE, 9, 0xB);
    gpio_cfg_nibble(GPIOA_BASE,10, 0xB);
    gpio_cfg_nibble(GPIOB_BASE,13, 0xB);
    gpio_cfg_nibble(GPIOB_BASE,14, 0xB);
    gpio_cfg_nibble(GPIOB_BASE,15, 0xB);
}
#endif

static void gate_enable_gpio_init(void) {
    gpio_cfg_nibble(GPIOB_BASE, 1, 0x2);
    GPIO_BRR(GPIOB_BASE) = (1u << 1);
}

static void power_stage_force_disarm(void) {
    GPIO_BRR(GPIOB_BASE) = (1u << 1);
    TIM_CCER(TIM1_BASE) &= ~GATE_CCER_MASK;
    gate_pins_to_safe_inputs();
    g_power_armed = 0;
}

static uint32_t gate_safety_check(void) {
    uint32_t ccer = TIM_CCER(TIM1_BASE);
    uint32_t ok = 1;
    if (!(ccer & ADC_CCER_MASK)) ok = 0;
    if (!(TIM_BDTR(TIM1_BASE) & TIM_BDTR_MOE)) ok = 0;

    if (g_power_armed) {
        if (!(GPIO_ODR(GPIOB_BASE) & (1u << 1))) ok = 0;
        if ((ccer & GATE_CCER_MASK) != GATE_CCER_MASK) ok = 0;
        if (gate_pin_mode_word_a() != 0xBBBu) ok = 0;
        if (gate_pin_mode_word_b() != 0xBBBu) ok = 0;
    } else {
        if (GPIO_ODR(GPIOB_BASE) & (1u << 1)) ok = 0;
        if (ccer & GATE_CCER_MASK) ok = 0;
        if (gate_pin_mode_word_a() != 0x888u) ok = 0;
        if (gate_pin_mode_word_b() != 0x888u) ok = 0;
    }

    if (!ok) {
        g_safety_latched = 1;
        power_stage_force_disarm();
    }
    return ok;
}

static void uart1_debug_init(void) {
    RCC_APB2ENR |= (1u << 14) | (1u << 3);
    gpio_cfg_nibble(GPIOB_BASE, 6, 0xB);
    gpio_cfg_nibble(GPIOB_BASE, 7, 0x4);
    USART_BRR = 556u;
    USART_CR1 = USART_CR1_UE | USART_CR1_TE | USART_CR1_RE;
}

static void uart_putc(char c) {
    while (!(USART_SR & USART_SR_TXE)) {}
    USART_DR = (uint32_t)(uint8_t)c;
}

static void uart_puts(const char *s) { while (*s) uart_putc(*s++); }

static void uart_u32(uint32_t v) {
    char b[10]; unsigned n=0;
    if (!v) { uart_putc('0'); return; }
    while (v && n < sizeof b) { b[n++] = (char)('0' + (v % 10u)); v /= 10u; }
    while (n) uart_putc(b[--n]);
}

static void uart_hex16(uint16_t v) {
    static const char h[]="0123456789ABCDEF";
    for (int s=12; s>=0; s-=4) uart_putc(h[(v >> s) & 0xF]);
}

static int uart_getc_nonblock(void) {
    if (!(USART_SR & USART_SR_RXNE)) return -1;
    return (int)(USART_DR & 0xFFu);
}

static uint16_t abs16s(int32_t x) {
    if (x < 0) x = -x;
    if (x > 65535) x = 65535;
    return (uint16_t)x;
}

static void adc1_calibrate_injected_offsets(void) {
    uint32_t sum0=0, sum1=0, sum2=0;
    ADC_CR1 &= ~ADC_CR1_JEOCIE;
    ADC_CR2 = (ADC_CR2 & ~ADC_CR2_JEXTSEL_MASK) |
              ADC_CR2_JEXTSEL_JSWSTART | ADC_CR2_JEXTTRIG | ADC_CR2_ADON;

    for (unsigned k=0; k<256u; k++) {
        ADC_SR &= ~ADC_SR_JEOC;
        ADC_CR2 |= ADC_CR2_JSWSTART;
        uint32_t start = DWT_CYCCNT;
        while (!(ADC_SR & ADC_SR_JEOC)) {
            if ((uint32_t)(DWT_CYCCNT - start) > 20000u) {
                g_safety_latched = 0xA001u;
                return;
            }
        }
        sum0 += (uint16_t)ADC_JDR1;
        sum1 += (uint16_t)ADC_JDR2;
        sum2 += (uint16_t)ADC_JDR3;
    }
    g_adc_offset[0]=(uint16_t)(sum0>>8);
    g_adc_offset[1]=(uint16_t)(sum1>>8);
    g_adc_offset[2]=(uint16_t)(sum2>>8);
    ADC_SR &= ~ADC_SR_JEOC;
}

static void adc1_injected_init(void) {
    RCC_APB2ENR |= (1u << 9) | (1u << 2);
    gpio_cfg_nibble(GPIOA_BASE,1,0x0);
    gpio_cfg_nibble(GPIOA_BASE,3,0x0);
    gpio_cfg_nibble(GPIOA_BASE,4,0x0);
    gpio_cfg_nibble(GPIOA_BASE,5,0x0);

    ADC_CR1 = ADC_CR1_SCAN;
    ADC_SMPR2 = (5u<<(1*3)) | (2u<<(3*3)) | (2u<<(4*3)) | (2u<<(5*3));
    ADC_JSQR = (3u << 20) | (3u << 0) | (4u << 5) | (5u << 10) | (1u << 15);
    ADC_CR2 = ADC_CR2_ADON;
    for (volatile unsigned i=0;i<1000;i++) nop();
    ADC_CR2 |= ADC_CR2_RSTCAL; while (ADC_CR2 & ADC_CR2_RSTCAL) {}
    ADC_CR2 |= ADC_CR2_CAL;    while (ADC_CR2 & ADC_CR2_CAL) {}

    adc1_calibrate_injected_offsets();

    ADC_CR2 = (ADC_CR2 & ~ADC_CR2_JEXTSEL_MASK) |
              ADC_CR2_JEXTSEL_TIM1_CC4 | ADC_CR2_JEXTTRIG | ADC_CR2_ADON;
    ADC_CR1 |= ADC_CR1_JEOCIE;
    ADC_SR &= ~ADC_SR_JEOC;
    NVIC_IPR8(IRQ_ADC1_2) = 0x20;
    NVIC_ISER0 = (1u << IRQ_ADC1_2);
}

static void tim1_pwm_and_adc_trigger_init(void) {
    RCC_APB2ENR |= (1u << 11);
    TIM_CR1(TIM1_BASE)=0;
    TIM_CR2(TIM1_BASE)=0;
    TIM_PSC(TIM1_BASE)=0;
    TIM_ARR(TIM1_BASE)=PWM_ARR;
    TIM_RCR(TIM1_BASE)=1;
    TIM_CCR1(TIM1_BASE)=PWM_ZERO_CCR;
    TIM_CCR2(TIM1_BASE)=PWM_ZERO_CCR;
    TIM_CCR3(TIM1_BASE)=PWM_ZERO_CCR;
    TIM_CCR4(TIM1_BASE)=ADC_TRIGGER_CCR;

    TIM_CCMR1(TIM1_BASE) = (6u<<4) | (1u<<3) | (6u<<12) | (1u<<11);
    TIM_CCMR2(TIM1_BASE) = (6u<<4) | (1u<<3) | (7u<<12) | (1u<<11);

    TIM_CCER(TIM1_BASE)=ADC_CCER_MASK;
    TIM_BDTR(TIM1_BASE)=DEADTIME_DTG | TIM_BDTR_OSSI | TIM_BDTR_OSSR | TIM_BDTR_MOE;
    TIM_EGR(TIM1_BASE)=TIM_EGR_UG;
    TIM_SR(TIM1_BASE)=0;
    TIM_CR1(TIM1_BASE)=TIM_CR1_CMS_0 | TIM_CR1_ARPE | TIM_CR1_CEN;
}

#if POWER_STAGE_ARM_ALLOWED
static uint32_t arm_preconditions_ok(void) {
    if (!POWER_STAGE_ARM_ALLOWED) return 0;
    if (g_ms < ARM_BOOT_DELAY_MS) return 0;
    if (g_adc_samples < ARM_MIN_ADC_SAMPLES) return 0;
    if (g_safety_latched) return 0;
    if ((uint32_t)(g_ms - g_adc_last_ms) > ADC_STALE_MS) return 0;
    if (g_last_abs_current > ARM_IDLE_CURRENT_COUNTS) return 0;
    if (g_adc_raw[3] < 50u || g_adc_raw[3] > 4050u) return 0;
    return 1;
}

static uint32_t power_stage_arm_zero_vector(void) {
    if (!arm_preconditions_ok()) return 0;

    TIM_CCR1(TIM1_BASE)=PWM_ZERO_CCR;
    TIM_CCR2(TIM1_BASE)=PWM_ZERO_CCR;
    TIM_CCR3(TIM1_BASE)=PWM_ZERO_CCR;
    gate_pins_to_tim1_af();
    TIM_CCER(TIM1_BASE) |= GATE_CCER_MASK;
    delay_cycles(SYSCLK_HZ / 1000u);

    if (g_last_abs_current > ARM_IDLE_CURRENT_COUNTS || g_safety_latched) {
        power_stage_force_disarm();
        return 0;
    }

    g_power_armed = 1;
    GPIO_BSRR(GPIOB_BASE) = (1u << 1);
    if (!gate_safety_check()) return 0;
    return 1;
}
#endif

void Reset_Handler(void) {
    uint32_t *src=&_sidata;
    for (uint32_t *dst=&_sdata; dst<&_edata;) *dst++=*src++;
    for (uint32_t *dst=&_sbss; dst<&_ebss;) *dst++=0;
    (void)main();
    for (;;) {}
}

void Default_Handler(void) {
    g_safety_latched = 0xDEAD0001u;
    power_stage_force_disarm();
    for (;;) {}
}

void SysTick_Handler(void) { g_ms++; }

void ADC1_2_IRQHandler(void) {
    uint32_t t0=DWT_CYCCNT;
    if (!(ADC_SR & ADC_SR_JEOC)) return;

    uint16_t s0=(uint16_t)ADC_JDR1;
    uint16_t s1=(uint16_t)ADC_JDR2;
    uint16_t s2=(uint16_t)ADC_JDR3;
    uint16_t s3=(uint16_t)ADC_JDR4;
    ADC_SR &= ~ADC_SR_JEOC;

    uint32_t now=t0;
    if (g_adc_samples) {
        uint32_t dt=now-g_last_sample_cycle;
        if (dt<g_sample_interval_min) g_sample_interval_min=dt;
        if (dt>g_sample_interval_max) g_sample_interval_max=dt;
    }
    g_last_sample_cycle=now;
    g_adc_last_ms=g_ms;
    g_adc_raw[0]=s0; g_adc_raw[1]=s1; g_adc_raw[2]=s2; g_adc_raw[3]=s3;

    int32_t ia=(int32_t)s0-(int32_t)g_adc_offset[0];
    int32_t ib=(int32_t)s1-(int32_t)g_adc_offset[1];
    int32_t ic=(int32_t)s2-(int32_t)g_adc_offset[2];
    uint16_t aa=abs16s(ia), ab=abs16s(ib), ac=abs16s(ic);
    uint16_t peak=aa; if (ab>peak) peak=ab; if (ac>peak) peak=ac;
    g_last_abs_current=peak;
    if (peak>g_peak_abs_current) g_peak_abs_current=peak;

    if (g_power_armed && peak>HARD_OC_COUNTS) {
        g_overcurrent_trips++;
        g_safety_latched=0x0C01u;
        power_stage_force_disarm();
    }

    if (++g_control_div4 >= 4u) {
        g_control_div4=0;
        phase_current_counts_t i;
        i.ia=(int16_t)ia; i.ib=(int16_t)ib; i.ic=(int16_t)ic;
        uint32_t c0=DWT_CYCCNT;
        control_diag_step(&ctrl,i,PWM_ARR);
        uint32_t cc=DWT_CYCCNT-c0;
        g_last_control_cycles=cc;
        if (cc>g_max_control_cycles) g_max_control_cycles=cc;
        if (cc>CONTROL_HARD_CYCLES) g_control_hard_overruns++;
        g_control_ticks++;

        if (g_power_armed) {
            TIM_CCR1(TIM1_BASE)=PWM_ZERO_CCR;
            TIM_CCR2(TIM1_BASE)=PWM_ZERO_CCR;
            TIM_CCR3(TIM1_BASE)=PWM_ZERO_CCR;
        }
    }

    g_adc_samples++;
    if ((g_adc_samples & 0x3Fu)==0u) (void)gate_safety_check();
    uint32_t isr=DWT_CYCCNT-t0;
    g_last_isr_cycles=isr;
    if (isr>g_max_isr_cycles) g_max_isr_cycles=isr;
}

static void reset_stats(void) {
    g_sample_interval_min=0xFFFFFFFFu;
    g_sample_interval_max=0;
    g_max_isr_cycles=0;
    g_max_control_cycles=0;
    g_peak_abs_current=0;
    g_control_hard_overruns=0;
}

static void report(void) {
    uart_puts("v0.4 PWM-SYNC ");
#if POWER_STAGE_ARM_ALLOWED
    uart_puts("ACTIVE-CAPABLE");
#else
    uart_puts("SYNC-SAFE");
#endif
    uart_puts(" arm="); uart_u32(g_power_armed);
    uart_puts(" samples="); uart_u32(g_adc_samples);
    uart_puts(" dt(min/max/exp)=");
    uart_u32(g_sample_interval_min==0xFFFFFFFFu?0u:g_sample_interval_min);
    uart_putc('/'); uart_u32(g_sample_interval_max); uart_putc('/'); uart_u32(PWM_SAMPLE_EXPECT_CYCLES);
    uart_puts(" ctrl(last/max)="); uart_u32(g_last_control_cycles); uart_putc('/'); uart_u32(g_max_control_cycles);
    uart_puts(" isr(last/max)="); uart_u32(g_last_isr_cycles); uart_putc('/'); uart_u32(g_max_isr_cycles);
    uart_puts(" Iabs(last/peak)="); uart_u32(g_last_abs_current); uart_putc('/'); uart_u32(g_peak_abs_current);
    uart_puts(" hard="); uart_u32(g_control_hard_overruns);
    uart_puts(" oc="); uart_u32(g_overcurrent_trips);
    uart_puts(" safe="); uart_u32(g_safety_latched);
    uart_puts(" raw="); uart_hex16(g_adc_raw[0]); uart_putc(','); uart_hex16(g_adc_raw[1]); uart_putc(','); uart_hex16(g_adc_raw[2]); uart_putc(','); uart_hex16(g_adc_raw[3]);
    uart_puts("\r\n");
}

static void handle_command(int ch) {
    if (ch=='a' || ch=='A') {
#if POWER_STAGE_ARM_ALLOWED
        if (g_power_armed) uart_puts("ARM: already armed\r\n");
        else if (power_stage_arm_zero_vector()) uart_puts("ARM: ZERO-VECTOR PWM ACTIVE\r\n");
        else uart_puts("ARM: refused by safety preconditions\r\n");
#else
        uart_puts("ARM: disabled in SYNC-SAFE build\r\n");
#endif
    } else if (ch=='d' || ch=='D') {
        power_stage_force_disarm();
        uart_puts("DISARMED\r\n");
    } else if (ch=='c' || ch=='C') {
        reset_stats();
        uart_puts("stats cleared\r\n");
    } else if (ch=='?' || ch=='h' || ch=='H') {
        uart_puts("commands: ? status/help, A arm zero-vector (active build only), D disarm, C clear stats\r\n");
        report();
    }
}

int main(void) {
    irq_disable();
    clock_64mhz_hsi();
    dwt_init();
    power_hold_init();
    gate_enable_gpio_init();
    gate_pins_to_safe_inputs();
    uart1_debug_init();
    control_diag_init(&ctrl);
    adc1_injected_init();
    tim1_pwm_and_adc_trigger_init();
    systick_init();
    irq_enable();

    uart_puts("DeltaESC G30D clean v0.4 PWM/ADC sync bring-up\r\n");
#if POWER_STAGE_ARM_ALLOWED
    uart_puts("BUILD: ACTIVE-CAPABLE, but boots DISARMED. Send A only after scope checks.\r\n");
#else
    uart_puts("BUILD: SYNC-SAFE. Power-stage arming compiled out.\r\n");
#endif
    uart_puts("TIM1 center 16kHz, ~1us deadtime, ADC1 injected IA/IB/IC/VBUS on TIM1_CH4, control every 4th sample = 4kHz\r\n");
    uart_puts("PA11 remains GPIO HIGH power-hold; CH4 is internal trigger only.\r\n");

    uint32_t next=1000u;
    for (;;) {
        int ch=uart_getc_nonblock();
        if (ch>=0) handle_command(ch);

        if ((int32_t)(g_ms-next)>=0) {
            next+=1000u;
            report();
            if ((uint32_t)(g_ms-g_adc_last_ms)>ADC_STALE_MS) {
                uart_puts("FAULT: ADC injected trigger stale; bridge forced disarmed\r\n");
                g_safety_latched=0xA002u;
                power_stage_force_disarm();
            }
            if (g_max_control_cycles>CONTROL_WARN_CYCLES) {
                uart_puts("WARN: 4kHz control reserve below 25%\r\n");
            }
        }

        if (!gate_safety_check()) {
            uart_puts("FAULT: PWM safety envelope violated; DISARMED\r\n");
        }
        __asm volatile("wfi");
    }
}
