#include <stdint.h>
#include "stm32f103_min.h"
#include "sensorless_control.h"
#include "motor_id.h"
#include "motor_id_auto.h"
#include "motor_id_vector_output.h"
#include "motor_id_adc_feedback.h"
#include "motor_id_flux_feedback.h"
#include "motor_pipeline.h"
#include "stock_tim1_gate.h"
#include "tim1_bdtr_guard.h"
#include "tim1_emergency_stop.h"
#include "g30_adc_proof.h"
#include "g30_rolling_bemf.h"
#include "g30_passive_bemf_adc.h"
#include "ninebot_link.h"
#include "iap_update.h"
#include "power_logic.h"
#include "motor_probe.h"
#include "ble_motor_probe.h"
#include "commissioning_guard.h"
#include "stock_current_frontend.h"
#include "g30_adc_current_window.h"
#include "g30_adc_current_offsets.h"
#include "stock_dual_adc_plan.h"
#include "stock_dual_adc_runtime.h"
#include "stock_adc_quality.h"
#include "stock_adc_snapshot.h"
#include "stock_adc_timing.h"
#include "stock_adc_correlated.h"
#include "stock_adc_mode_guard.h"
#include "g30_fixed_scan.h"
#include "motor_config_txn.h"
#include "dashboard_runtime.h"
#include "drive_runtime.h"
#include "g30_target_profile.h"

#ifndef DRIVE_RUNTIME_ALLOWED
#define DRIVE_RUNTIME_ALLOWED 0
#endif

#ifndef POWER_STAGE_ARM_ALLOWED
#define POWER_STAGE_ARM_ALLOWED 0
#endif
#ifndef SENSORLESS_RUN_ALLOWED
#define SENSORLESS_RUN_ALLOWED 0
#endif
#ifndef MOTOR_ID_BENCH_ALLOWED
#define MOTOR_ID_BENCH_ALLOWED 0
#endif
#if MOTOR_ID_BENCH_ALLOWED && (POWER_STAGE_ARM_ALLOWED || SENSORLESS_RUN_ALLOWED)
#error "Motor-ID bench must never share the drive-power build"
#endif

#define SYSCLK_HZ                  64000000u
#define PWM_HZ                     16000u
#define CONTROL_HZ                  4000u
#define PWM_ARR                     4000u /* stock DRV126 TIM1 ARR */
#define PWM_ZERO_CCR               ((PWM_ARR + 1u) / 2u)
#define ADC_TRIGGER_CCR             3996u /* stock DRV126 CC4 / PWM1 */
#define CONTROL_BUDGET_CYCLES      (SYSCLK_HZ / CONTROL_HZ)
#define CONTROL_WARN_CYCLES         12000u
#define CONTROL_HARD_CYCLES         15000u
#define PWM_SAMPLE_EXPECT_CYCLES    (SYSCLK_HZ / PWM_HZ)
#define DEADTIME_DTG                  64u
#define ARM_BOOT_DELAY_MS            3000u
#define ARM_MIN_ADC_SAMPLES          2000u
#define ARM_IDLE_CURRENT_COUNTS       120u
#if ROLLING_SENSORLESS_RC_ALLOWED
#define HARD_OC_COUNTS                 96u /* ~4.8 A on stock nominal count model; raw-count trip */
#else
#define HARD_OC_COUNTS                700u
#endif
#define ADC_STALE_MS                    5u
#define IRQ_ADC1_2 18u
#define IRQ_TIM1_UP 25u
#define IRQ_USART2 38u
#define GATE_CCER_MASK STOCK_TIM1_GATE_CH_MASK
#define ADC_CCER_MASK STOCK_TIM1_GATE_ADC_CC4E
#define MAGIC_UNLOCK 0xC0DEu
#define PROTO_MAJOR 0u
#define PROTO_MINOR 2u
/* Explicit identity for this source candidate; do not masquerade as a
 * previously hardware-tested image or frozen Android pairing. */
#define FW_BUILD 0x1030u
/* v0.9.20 compiles the complete sensorless software path for Cortex-M3,
 * but the physical bridge remains compile-locked. E5 therefore cannot arm
 * the G30 power stage in this target. */
#if POWER_STAGE_ARM_ALLOWED && !ROLLING_SENSORLESS_RC_ALLOWED
#error "v0.10.1: physical gate arm permitted only by explicit rolling RC target"
#endif
#define COMM_DASH_STALE_MS 250u
#define COMM_INPUT_IDLE_MAX 3u
#ifndef PASSIVE_BEMF_PROBE_ALLOWED
#define PASSIVE_BEMF_PROBE_ALLOWED 0
#endif
#ifndef ROLLING_SENSORLESS_RC_ALLOWED
#define ROLLING_SENSORLESS_RC_ALLOWED 0
#endif
#define FO70_R_UOHM 583000u
#define FO70_L_NH 640000u
#define FO70_FLUX_UWB 12100u
#define FO70_FRAME_OFFSET_U16 21327
#define FO70_RUN_CURRENT_MA 600u
/* Gate I/O mapping is adopted from STOCK DRV126. This alone does not
 * calibrate phase currents, validate ADC sample windows, or prove OC protection. */
#define COMM_CURRENT_SCALE_HW_VALID 0u /* physical mA/count unverified, no motor activation */
#define COMM_GATE_STOCK_PROFILE_READY 1u /* DRV126 CCER=0x1555 and six TIM1 CHx/CHxN pins */
#define COMM_ADC_TIMING_HW_VALID 0u /* analogue sampling aperture not motor-qualified */
#define COMM_STOCK_CURRENT_MODEL_VALID 1u
/* Live bench is explicit opt-in and cannot enable regular torque driving.
 * Fast raw-ADC trip is independent of the provisional mA/count model.
 * The small low-current bench envelope is NOT a general motor qualification. */
#define ID_BENCH_RAW_TRIP_COUNTS 24u
#define ID_BENCH_MAX_ACTIVE_MS 2500u
#define ID_BENCH_MAX_TOTAL_MS 3600u
#define ID_BENCH_TOKEN 0x4D49u /* "IM" after the existing 0xC0DE magic */
#define COMM_STOCK_DUAL_ADC_PLAN_VALID 1u
#if MOTOR_VOLTAGE_BENCH
#error "v0.9.5: motor-voltage build not allowed"
#endif
/* No more blanket "unknown gate pinout" veto. The gate backend is
 * now taken from DRV126. Power builds are blocked ONLY by uncalibrated
 * current sensing and ADC timing, separate from Hall vs sensorless. */
/* SENSORLESS_RUN_ALLOWED may be compiled for source/link proof with the
 * bridge hard-disabled. Only a build that can physically arm the bridge must
 * satisfy the not-yet-qualified current-scale and ADC-timing gates. */
/* Production still requires fully qualified calibration flags. The explicit
 * rolling RC instead uses the stock DRV126 nominal count model plus SAME-TICK
 * runtime window/offset guards and a much lower raw-count trip. */
#if POWER_STAGE_ARM_ALLOWED && !ROLLING_SENSORLESS_RC_ALLOWED
#if !COMM_CURRENT_SCALE_HW_VALID || !COMM_ADC_TIMING_HW_VALID
#error "DRV126 gate profile ready; powered build still requires valid ADC timing and current scaling"
#endif
#endif
#define STOCK_COMPAT_FW_WORD 0x0420u
#define POWER_HOLD_PIN 11u
#define POWER_BUTTON_PIN 14u
#define POWER_BUTTON_GPIO_BASE GPIOC_BASE
#define POWER_BUTTON_LONG_MS 6000u
#define POWER_OFF_ACK_DELAY_MS 150u
#define OFFSET_CAL_SAMPLES 512u

#define ACT_OK 0u
#define ACT_BUSY 1u
#define ACT_UNSAFE 2u
#define ACT_BAD_MAGIC 3u
#define ACT_UNSUPPORTED 4u
#define ACT_BAD_RANGE 5u
#define ACT_RAM_ONLY 6u

static power_button_state_t g_power_button_state;
static volatile uint8_t g_poweroff_requested;
static volatile uint32_t g_poweroff_deadline_ms;
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

static sensorless_control_t ctrl;
/* Motor Detect has its own PWM ownership; ADC feedback is a separate task.
 * The source-only build continues to deny physical bridge activation. */
static motor_id_auto_t g_motor_id_auto;
static motor_id_vector_output_t g_motor_id_vector;
static motor_id_adc_feedback_t g_motor_id_feedback;
static motor_id_flux_feedback_t g_motor_id_flux_feedback;
static volatile uint8_t g_motor_id_vector_owner;
static volatile uint32_t g_motor_id_vector_last_ms;
static volatile uint32_t g_id_bench_started_ms;
static volatile uint32_t g_id_bench_energized_since_ms;
static volatile uint8_t g_id_bench_requested;
/* Once STOP/E6/fault requests the hardware kill, timer rearm is prohibited
 * until a full MCU reset. Do not reset this latch via BLE or diagnostics. */
static volatile uint8_t g_gate_kill_latched;
static motor_pipeline_t g_motor_pipeline;
static g30_rolling_bemf_t g_rolling_bemf;
static g30_passive_bemf_adc_t g_passive_bemf_adc;
static volatile uint32_t g_motor_irq_sequence;
static volatile uint8_t g_offset_cal_active;
static volatile uint8_t g_offset_cal_valid;
static volatile uint16_t g_adc2_ch4_offset;
static volatile uint16_t g_adc2_ch5_offset;
static volatile uint8_t g_adc2_ch4_offset_valid;
static volatile uint8_t g_adc2_ch5_offset_valid;
static stock_dual_adc_runtime_t g_dual_adc;
static stock_adc_quality_t g_adc_quality;
static stock_adc_snapshot_t g_pair_snapshot;
static stock_adc_timing_t g_timer_evidence;
static stock_adc_correlated_t g_correlated_evidence;
static g30_adc_mode_guard_t g_adc_mode_guard;
static g30_fixed_scan_t g_fixed_scan;
static volatile uint16_t g_last_adc1_pair,g_last_adc2_pair;
static volatile uint32_t g_vbus_last_ms,g_vbus_start_ms;
static volatile uint8_t g_vbus_pending;
static volatile uint16_t g_vbus_timeout_events;
static volatile uint8_t g_last_pair_sector;
static volatile uint8_t g_last_action_status;
static volatile uint8_t g_dash_seen;
static volatile uint8_t g_dash_throttle;
static volatile uint8_t g_dash_brake;
static volatile uint32_t g_dash_frames;
static volatile uint32_t g_dash_last_ms;
static uint32_t g_cfg_r_uohm;
static uint32_t g_cfg_l_nh;
static uint32_t g_cfg_flux_uwb;
static int16_t g_cfg_phase_offset;
static uint16_t g_cfg_test_current_ma = 500u;
static uint8_t g_cfg_dirty;
static motor_config_txn_t g_motor_cfg;
static uint8_t g_phase_map_valid;
static uint8_t g_phase_map[3] = {0xFFu,0xFFu,0xFFu};
static uint8_t g_phase_signs;
static uint8_t g_phase_direction;
static volatile uint8_t g_shu_handoff_refused;

extern uint32_t _estack, _sidata, _sdata, _edata, _sbss, _ebss;
int main(void);
void Reset_Handler(void);
void Default_Handler(void);
void SysTick_Handler(void);
void ADC1_2_IRQHandler(void);
void TIM1_UP_IRQHandler(void);
void USART2_IRQHandler(void);
static void power_stage_force_disarm(void);

__attribute__((section(".isr_vector"), used))
void (* const vectors[16u + IRQ_USART2 + 1u])(void) = {
    [0] = (void (*)(void))(&_estack),
    [1] = Reset_Handler,
    [2] = Default_Handler, [3] = Default_Handler, [4] = Default_Handler,
    [5] = Default_Handler, [6] = Default_Handler, [7] = Default_Handler,
    [8] = Default_Handler, [9] = Default_Handler, [10] = Default_Handler,
    [11] = Default_Handler, [12] = Default_Handler, [13] = Default_Handler,
    [14] = Default_Handler, [15] = SysTick_Handler,
    [16u + IRQ_ADC1_2] = ADC1_2_IRQHandler,
    [16u + IRQ_TIM1_UP] = TIM1_UP_IRQHandler,
    [16u + IRQ_USART2] = USART2_IRQHandler
};

static void gpio_cfg_nibble(uint32_t base, unsigned pin, uint32_t nibble) {
    volatile uint32_t *reg = (volatile uint32_t *)((pin < 8u) ? (base + 0x00u) : (base + 0x04u));
    unsigned shift = (pin & 7u) * 4u;
    uint32_t v = *reg;
    v = (v & ~(0xFu << shift)) | ((nibble & 0xFu) << shift);
    *reg = v;
}

static uint32_t gate_pin_mode_word_a(void) { return (GPIO_CRH(GPIOA_BASE) >> 0) & 0xFFFu; }
static uint32_t gate_pin_mode_word_b(void) { return (GPIO_CRH(GPIOB_BASE) >> 20) & 0xFFFu; }

static void clock_64mhz_hsi(void) {
    RCC_CR |= (1u << 0); while (!(RCC_CR & (1u << 1))) {}
    FLASH_ACR = (1u << 4) | 2u;
    uint32_t cfgr = 0u;
    cfgr |= (4u << 8); cfgr |= (2u << 14); cfgr |= (14u << 18);
    RCC_CFGR = cfgr;
    RCC_CR |= (1u << 24); while (!(RCC_CR & (1u << 25))) {}
    RCC_CFGR = (RCC_CFGR & ~3u) | 2u;
    while (((RCC_CFGR >> 2) & 3u) != 2u) {}
}

static void dwt_init(void) { COREDEBUG_DEMCR |= (1u << 24); DWT_CYCCNT = 0u; DWT_CTRL |= 1u; }
static void systick_init(void) { SYST_RVR = (SYSCLK_HZ / 1000u) - 1u; SYST_CVR = 0u; SYST_CSR = 7u; }


static void power_hold_init(void) {
    RCC_APB2ENR |= (1u << 0) | (1u << 2) | (1u << 3) | (1u << 4);
    /* DRV126 + board pinout: PA11 = power latch, PC14 = active-HIGH
       pushbutton (physical LQFP48 pin 3). PA12 is NOT the power button.
       Preload PA11 HIGH before changing its mode to avoid an output-low glitch.
       PC14 stays high-impedance input; never program it as an output. */
    GPIO_BSRR(GPIOA_BASE) = (1u << POWER_HOLD_PIN);
    gpio_cfg_nibble(GPIOA_BASE, POWER_HOLD_PIN, 0x2u);
    gpio_cfg_nibble(POWER_BUTTON_GPIO_BASE, POWER_BUTTON_PIN, 0x4u); /* floating input */
}
static void gate_pins_to_safe_inputs(void) {
    /* Set STM32F1 ODR LOW *before* configuring input with pull-up/down.
       On STM32F103 0x8 means input-pull; ODR=0 selects pull-DOWN.
       MT8006A HIN/LIN require LOW for both MOSFETs off. */
    GPIO_BRR(GPIOA_BASE)=(1u<<8)|(1u<<9)|(1u<<10);
    GPIO_BRR(GPIOB_BASE)=(1u<<13)|(1u<<14)|(1u<<15);
    gpio_cfg_nibble(GPIOA_BASE,8,0x8u); gpio_cfg_nibble(GPIOA_BASE,9,0x8u); gpio_cfg_nibble(GPIOA_BASE,10,0x8u);
    gpio_cfg_nibble(GPIOB_BASE,13,0x8u); gpio_cfg_nibble(GPIOB_BASE,14,0x8u); gpio_cfg_nibble(GPIOB_BASE,15,0x8u);
    GPIO_BRR(GPIOA_BASE)=(1u<<8)|(1u<<9)|(1u<<10); GPIO_BRR(GPIOB_BASE)=(1u<<13)|(1u<<14)|(1u<<15);
}
#if POWER_STAGE_ARM_ALLOWED || MOTOR_ID_BENCH_ALLOWED
static void gate_pins_to_tim1_af(void) {
    gpio_cfg_nibble(GPIOA_BASE,8,0xBu); gpio_cfg_nibble(GPIOA_BASE,9,0xBu); gpio_cfg_nibble(GPIOA_BASE,10,0xBu);
    gpio_cfg_nibble(GPIOB_BASE,13,0xBu); gpio_cfg_nibble(GPIOB_BASE,14,0xBu); gpio_cfg_nibble(GPIOB_BASE,15,0xBu);
}
#endif
/* PB1 is BEMF_C on the G30D board, NOT a gate-enable output.
 * Never configure or drive PB1 as part of the motor power path. */
/* Physical gate isolation is shared by STOP, BRAKE, throttle idle,
 * observer fault, ADC fault, shutdown, and dashboard watchdog. Never
 * confuse the 50% duty software zero vector with hardware coast. */
static void power_stage_force_disarm(void) {
    /* EBiCS motor_disable_pwm() principle. Every forced STOP is terminal:
     * loss of ADC-CH4 injected trigger is acceptable on a latched fault.
     * This flag is written BEFORE any timer/GPIO accesses. */
    g_gate_kill_latched=1u;
    g_id_bench_energized_since_ms=0u;
    TIM_DIER(TIM1_BASE)&=~1u; /* TIM1 update IRQ only for Motor-ID PWM */
    motor_id_adc_feedback_off(&g_motor_id_feedback);
    /* First disconnect all three complementary TIM1 pairs, then remove
     * MOE. Unlike passive CH4 measurement this is a terminal hardware kill.
     * AOE=0 must already have been established by v0.9.16 initialization.
     * Never write AOE/BKP/BKE/LOCK based on an unvalidated AMP_LIM pin. */
    TIM_CCER(TIM1_BASE)=stock_tim1_gate_ccer_off((uint16_t)TIM_CCER(TIM1_BASE));
    TIM_BDTR(TIM1_BASE)&=(uint32_t)~D_GUARD_MOE;
    gate_pins_to_safe_inputs();
    if (!tim1_emergency_registers_off((uint16_t)TIM_BDTR(TIM1_BASE),
                                       (uint16_t)TIM_CCER(TIM1_BASE)) ||
        !tim1_bdtr_guard_gpio_down((uint16_t)gate_pin_mode_word_a(),
                                   (uint16_t)gate_pin_mode_word_b(),
                                   (uint16_t)GPIO_ODR(GPIOA_BASE),
                                   (uint16_t)GPIO_ODR(GPIOB_BASE))) {
        g_safety_latched=0xA02Cu; /* failed emergency hardware readback */
    }
    __asm volatile("dsb" ::: "memory");
    if (g_power_armed) {
        /* Restore the disabled bridge's two-rank read-only ADC diagnostics. */
        ADC_JSQR=G30_FIXED_JSQR_A;
        ADC2_JSQR=G30_FIXED_JSQR_B;
    }
    g_power_armed=0u;
    g_motor_id_vector_owner=0u;
    g_motor_id_vector.bridge_armed=0u;
    g_motor_id_vector.owning=0u;
    g_motor_id_vector.sector=0u;
    g_motor_pipeline.powered=0u;
    g_motor_pipeline.adc.pending=0u;
    sensorless_control_stop(&ctrl);
}

/* Motor ID L/R/flux normal pause. This is NOT a terminal emergency.
 * The R/L sequence needs repeated re-arming after a 4ms coast interval.
 * TIM1 CH4 ADC trigger uses MOE on F103; keep it in the passive state.
 * Never invoke on a terminal fault to undo a hardware stop. */
static void power_stage_normal_coast(void) {
    if (g_gate_kill_latched || g_safety_latched) {
        power_stage_force_disarm();
        return;
    }
    g_id_bench_energized_since_ms=0u;
    TIM_DIER(TIM1_BASE)&=~1u;
    motor_id_adc_feedback_off(&g_motor_id_feedback);
    TIM_CCER(TIM1_BASE)=stock_tim1_gate_ccer_off((uint16_t)TIM_CCER(TIM1_BASE));
    gate_pins_to_safe_inputs();
    __asm volatile("dsb" ::: "memory");
    if (g_power_armed) {
        ADC_JSQR=G30_FIXED_JSQR_A;
        ADC2_JSQR=G30_FIXED_JSQR_B;
    }
    g_power_armed=0u;
    g_motor_id_vector_owner=0u;
    g_motor_id_vector.bridge_armed=0u;
    g_motor_id_vector.owning=0u;
    g_motor_id_vector.sector=0u;
    g_motor_pipeline.powered=0u;
    g_motor_pipeline.adc.pending=0u;
    /* If CH4 did not stay active, this is a fault, NOT a pause. */
    if (!tim1_bdtr_guard_passive((uint16_t)TIM_BDTR(TIM1_BASE),
                                 (uint8_t)DEADTIME_DTG,
                                 (uint16_t)TIM_CCER(TIM1_BASE)) ||
        !tim1_bdtr_guard_gpio_down((uint16_t)gate_pin_mode_word_a(),
                                   (uint16_t)gate_pin_mode_word_b(),
                                   (uint16_t)GPIO_ODR(GPIOA_BASE),
                                   (uint16_t)GPIO_ODR(GPIOB_BASE))) {
        g_safety_latched=0xA02Eu;
        power_stage_force_disarm();
    }
}

static void motor_id_bench_coast(void) {
    power_stage_normal_coast();
    sensorless_control_stop(&ctrl);
}

static void motor_hw_coast(void *ignored) {
    (void)ignored;
    power_stage_normal_coast();
    g30_rolling_bemf_reset(&g_rolling_bemf);
}

static void motor_hw_gate_off(void *ignored) {
    (void)ignored; power_stage_force_disarm();
}

static uint8_t motor_hw_select_adc(void *ignored,uint32_t jsqr1,uint32_t jsqr2) {
    (void)ignored;
    /* One injected rank on each ADC, both driven by the same TIM1 CC4
     * event. Called only after JEOC is captured/cleared or with IRQ masked. */
    ADC_JSQR=jsqr1; ADC2_JSQR=jsqr2;
    __asm volatile("dsb" ::: "memory");
    return (uint8_t)(ADC_JSQR==jsqr1 && ADC2_JSQR==jsqr2);
}

static uint8_t motor_hw_pwm(void *ignored,uint16_t c1,uint16_t c2,uint16_t c3) {
    (void)ignored;
    /* Conservative low-side shunt window: all 3 CCRs must leave
     * TIM1_CC4 enough settle + deadtime + ADC conversion room.
     * Without this, a future sector switch can invalidate the next sample. */
    if (g_gate_kill_latched) return 0u;
    if (c1>PWM_ARR || c2>PWM_ARR || c3>PWM_ARR ||
        c1>3744u || c2>3744u || c3>3744u ||
        g_safety_latched || g_dash_brake ||
        (g_motor_id_vector_owner ? g_dash_throttle>COMM_INPUT_IDLE_MAX :
                                   g_dash_throttle<=COMM_INPUT_IDLE_MAX))
        return 0u;
    if (!g_power_armed && (c1!=PWM_ZERO_CCR ||
                           c2!=PWM_ZERO_CCR || c3!=PWM_ZERO_CCR))
        return 0u; /* cannot write torque CCR after asynchronous gate kill */
    /* Preload registers are enabled for all channels; the timer's update
     * boundary applies the triple instead of toggling outputs mid-period. */
    TIM_CCR1(TIM1_BASE)=c1;
    TIM_CCR2(TIM1_BASE)=c2;
    TIM_CCR3(TIM1_BASE)=c3;
    return 1u;
}

static uint8_t motor_hw_arm(void *ignored) {
    if (g_gate_kill_latched) return 0u;
    (void)ignored;
#if POWER_STAGE_ARM_ALLOWED || MOTOR_ID_BENCH_ALLOWED
    if (!COMM_GATE_STOCK_PROFILE_READY || g_safety_latched ||
        (ROLLING_SENSORLESS_RC_ALLOWED &&
         (!g_offset_cal_valid || !g_adc2_ch4_offset_valid || !g_adc2_ch5_offset_valid)) ||
        (!g_id_bench_requested && !ROLLING_SENSORLESS_RC_ALLOWED &&
         (!COMM_CURRENT_SCALE_HW_VALID || !COMM_ADC_TIMING_HW_VALID)) ||
        (g_id_bench_requested &&
         (!MOTOR_ID_BENCH_ALLOWED || !g_offset_cal_valid ||
          !g_adc2_ch4_offset_valid || !g_adc2_ch5_offset_valid)) ||
        !g_dash_seen || g_dash_brake ||
        (g_motor_id_vector_owner ?
            (g_dash_throttle>COMM_INPUT_IDLE_MAX ||
             g_motor_pipeline.powered || ctrl.drive_request ||
             g_motor_id_auto.state<ID_AUTO_R_SETTLE ||
             g_motor_id_auto.state>ID_AUTO_FLUX_COAST) :
            (g_dash_throttle<=COMM_INPUT_IDLE_MAX)) ||
        (uint32_t)(g_ms-g_adc_last_ms)>ADC_STALE_MS ||
        (uint32_t)(g_ms-g_vbus_last_ms)>100u)
        return 0u;
    TIM_CCR1(TIM1_BASE)=PWM_ZERO_CCR;
    TIM_CCR2(TIM1_BASE)=PWM_ZERO_CCR;
    TIM_CCR3(TIM1_BASE)=PWM_ZERO_CCR;
    /* Install neutral duty into the ACTUAL active CCR shadow before any
     * output enable, not just into the preload registers. Without UG,
     * a previous powered command could reappear for one PWM period. */
    TIM_EGR(TIM1_BASE)=TIM_EGR_UG;
    TIM_SR(TIM1_BASE)&=~1u;
    ADC_SR&=~ADC_SR_JEOC;
    ADC2_SR&=~ADC_SR_JEOC;
    gate_pins_to_tim1_af();
    /* Stock DRV126 turns all six complementary gates on with 0x1555.
       A full store clears stale CCx polarity bits. */
    TIM_CCER(TIM1_BASE)=stock_tim1_gate_ccer_arm((uint16_t)TIM_CCER(TIM1_BASE));
    if (!stock_tim1_gate_ccer_is_armed((uint16_t)TIM_CCER(TIM1_BASE))) {
        power_stage_force_disarm();
        return 0u;
    }
    g_power_armed=1u;
    if(g_id_bench_requested && !g_id_bench_energized_since_ms)
        g_id_bench_energized_since_ms=g_ms;
    return 1u;
#else
    return 0u; /* no reachable physical gate arm in source-only builds */
#endif
}

/* Point 2.1: v0.9.10 motor ID voltage actuator. This callback is
 * deliberately NON-NULL and routes to the real sector ADC pair, TIM1
 * preload and stock gate backend; it does NOT synthesize current feedback.
 * Existing compile-time ADC/current qualification remains a hard barrier.
 */
static void motor_id_hw_gate_off(void *ignored) {
    (void)ignored;
    motor_id_bench_coast();
    g_motor_id_vector.bridge_armed=0u;
    g_motor_id_vector.owning=0u;
    g_motor_id_vector.sector=0u;
}
static uint8_t motor_id_hw_vector_mv(void *ignored,int32_t amplitude_mv,
                                     uint16_t angle_u16) {
    (void)ignored;
#if POWER_STAGE_ARM_ALLOWED || MOTOR_ID_BENCH_ALLOWED
    if (!g_id_bench_requested ||
        (!MOTOR_ID_BENCH_ALLOWED && (!COMM_CURRENT_SCALE_HW_VALID || !COMM_ADC_TIMING_HW_VALID)) ||
        (MOTOR_ID_BENCH_ALLOWED &&
         (!g_offset_cal_valid || !g_adc2_ch4_offset_valid || !g_adc2_ch5_offset_valid)) ||
        (uint32_t)(g_ms-g_id_bench_started_ms)>ID_BENCH_MAX_TOTAL_MS ||
        (g_id_bench_energized_since_ms &&
         (uint32_t)(g_ms-g_id_bench_energized_since_ms)>ID_BENCH_MAX_ACTIVE_MS) ||
        g_safety_latched || g_poweroff_requested ||
        iap_update_active() || iap_update_pending() ||
        !g_dash_seen || g_dash_brake ||
        g_dash_throttle>COMM_INPUT_IDLE_MAX ||
        (uint32_t)(g_ms-g_dash_last_ms)>COMM_DASH_STALE_MS ||
        (uint32_t)(g_ms-g_vbus_last_ms)>100u ||
        g_motor_pipeline.powered || ctrl.drive_request ||
        g_motor_id_auto.state<ID_AUTO_R_SETTLE ||
        g_motor_id_auto.state>ID_AUTO_FLUX_COAST ||
        (g_power_armed && !g_motor_id_vector_owner)) {
        motor_id_vector_output_stop(&g_motor_id_vector);
        return 0u;
    }
    /* Actual 10S VBUS measurement comes from existing ADC1 regular PA1.
     * The conversion gain is still not physically qualified for motor power. */
    const uint32_t vbus_mv=(uint32_t)g_adc_raw[3]*14u;
    uint32_t prior_primask;
    __asm volatile("mrs %0, primask" : "=r"(prior_primask) :: "memory");
    irq_disable();
    g_motor_id_vector_owner=1u; /* claim before invoking shared arm */
    const uint8_t ok=motor_id_vector_output_request(&g_motor_id_vector,
        amplitude_mv,angle_u16,vbus_mv,PWM_ARR,
        1u,0u,(uint8_t)(g_dash_brake || g_safety_latched));
    if(!ok) {
        power_stage_force_disarm();
    } else {
        /* CCR writes are PRELOADS. Publish only on the following physical
         * TIM1 update IRQ, never at vector_mv() call time. Clearing UIF
         * after all three writes prevents a previously pending update from
         * certifying a vector that was not yet loaded by the timer. */
        motor_id_adc_feedback_queue(&g_motor_id_feedback,
            (uint16_t)TIM_CCR1(TIM1_BASE),
            (uint16_t)TIM_CCR2(TIM1_BASE),
            (uint16_t)TIM_CCR3(TIM1_BASE),angle_u16,
            g_motor_id_vector.sector,g_motor_id_auto.command_epoch+1u);
        TIM_SR(TIM1_BASE)&=~1u;
        TIM_DIER(TIM1_BASE)|=1u;
        g_motor_id_vector_last_ms=g_ms;
    }
    if(!(prior_primask&1u))irq_enable();
    return ok;
#else
    (void)amplitude_mv;(void)angle_u16;
    /* This build has never been armed on G30D hardware; no fake success. */
    return 0u;
#endif
}

static void schedule_poweroff(uint32_t delay_ms) {
    power_stage_force_disarm();
    g_poweroff_requested=1u;
    g_poweroff_deadline_ms=g_ms+delay_ms;
}
static void power_hold_release_now(void) {
    power_stage_force_disarm();
    GPIO_BRR(GPIOA_BASE) = (1u << POWER_HOLD_PIN);
    for(;;) { __asm volatile("wfi"); }
}


static uint32_t gate_safety_check(void) {
    if (g_gate_kill_latched) {
        const uint8_t confirmed=(uint8_t)(
            tim1_emergency_registers_off((uint16_t)TIM_BDTR(TIM1_BASE),
                                          (uint16_t)TIM_CCER(TIM1_BASE)) &&
            tim1_bdtr_guard_gpio_down((uint16_t)gate_pin_mode_word_a(),
                                      (uint16_t)gate_pin_mode_word_b(),
                                      (uint16_t)GPIO_ODR(GPIOA_BASE),
                                      (uint16_t)GPIO_ODR(GPIOB_BASE)));
        if (!confirmed) {
            g_safety_latched=0xA02Du;
            /* Idempotent hardware kill. Do not recursively call ourselves. */
            TIM_CCER(TIM1_BASE)=stock_tim1_gate_ccer_off(
                                 (uint16_t)TIM_CCER(TIM1_BASE));
            TIM_BDTR(TIM1_BASE)&=(uint32_t)~D_GUARD_MOE;
            gate_pins_to_safe_inputs();
        }
        return confirmed;
    }
    uint32_t ccer=TIM_CCER(TIM1_BASE), ok=1u;
    if (!(ccer&ADC_CCER_MASK)) ok=0u;
    if (!(TIM_BDTR(TIM1_BASE)&TIM_BDTR_MOE)) ok=0u;
    /* Runtime assertion: any AOE/BKE/BKP/LOCK change or unexpected
       CH1..3 / complementary enable latches a software safety fault. */
    if (g_power_armed) {
        /* Live E7 only: six gate channels, CH4 trigger, AOE=0,
         * no BKIN/LOCK guesses, exactly the expected deadtime. */
        const uint16_t expected=(uint16_t)(DEADTIME_DTG |
            D_GUARD_OSSI | D_GUARD_OSSR | D_GUARD_MOE);
        if ((uint16_t)TIM_BDTR(TIM1_BASE)!=expected ||
            !stock_tim1_gate_ccer_is_armed((uint16_t)ccer) ||
            !drive_armed_owner_valid(MOTOR_ID_BENCH_ALLOWED,g_id_bench_requested,
                g_motor_id_vector_owner,
                (uint8_t)(g_motor_id_auto.state>=ID_AUTO_R_SETTLE &&
                          g_motor_id_auto.state<=ID_AUTO_FLUX_COAST),
                POWER_STAGE_ARM_ALLOWED && SENSORLESS_RUN_ALLOWED,
                g_motor_pipeline.powered,ctrl.drive_request)) ok=0u;
    } else if (!tim1_bdtr_guard_passive((uint16_t)TIM_BDTR(TIM1_BASE),
                                        (uint8_t)DEADTIME_DTG,
                                        (uint16_t)ccer)) ok=0u;
    if (g_power_armed) {
        if (!(g_motor_id_vector_owner ?
                (!g_motor_pipeline.powered && !ctrl.drive_request &&
                 g_motor_id_auto.state>=ID_AUTO_R_SETTLE &&
                 g_motor_id_auto.state<=ID_AUTO_FLUX_COAST) :
                g_motor_pipeline.powered) ||
            !COMM_GATE_STOCK_PROFILE_READY ||
            ((!COMM_CURRENT_SCALE_HW_VALID || !COMM_ADC_TIMING_HW_VALID) &&
             !(MOTOR_ID_BENCH_ALLOWED && g_id_bench_requested) &&
             !ROLLING_SENSORLESS_RC_ALLOWED)) ok=0u;
        if (!stock_tim1_gate_ccer_is_armed((uint16_t)ccer)) ok=0u;
        if (gate_pin_mode_word_a()!=0xBBBu || gate_pin_mode_word_b()!=0xBBBu) ok=0u;
    } else {
        if (!stock_tim1_gate_ccer_is_off((uint16_t)ccer)) ok=0u;
        if (!tim1_bdtr_guard_gpio_down((uint16_t)gate_pin_mode_word_a(),
                                        (uint16_t)gate_pin_mode_word_b(),
                                        (uint16_t)GPIO_ODR(GPIOA_BASE),
                                        (uint16_t)GPIO_ODR(GPIOB_BASE))) ok=0u;
    }
    if (!ok) { g_safety_latched=1u; power_stage_force_disarm(); }
    return ok;
}

static void uart1_debug_init(void) {
    RCC_APB2ENR |= (1u<<14)|(1u<<3); gpio_cfg_nibble(GPIOB_BASE,6,0xBu); gpio_cfg_nibble(GPIOB_BASE,7,0x4u);
    USART_BRR=556u; USART_CR1=USART_CR1_UE|USART_CR1_TE|USART_CR1_RE;
}
static void uart_putc(char c) { while (!(USART_SR&USART_SR_TXE)) {} USART_DR=(uint32_t)(uint8_t)c; }
static void uart_puts(const char *s) { while (*s) uart_putc(*s++); }
static void uart_u32(uint32_t v) { char b[10]; unsigned n=0u; if(!v){uart_putc('0');return;} while(v&&n<sizeof b){b[n++]=(char)('0'+(v%10u));v/=10u;} while(n)uart_putc(b[--n]); }
static int uart_getc_nonblock(void) { if(!(USART_SR&USART_SR_RXNE))return -1; return (int)(USART_DR&0xFFu); }
static uint16_t abs16s(int32_t x) { if(x<0)x=-x; if(x>65535)x=65535; return (uint16_t)x; }

static void adc1_calibrate_injected_offsets(void) {
    uint32_t sum0=0u,sum1=0u,sum2=0u;
    ADC_CR1&=~ADC_CR1_JEOCIE;
    ADC_CR2=(ADC_CR2&~ADC_CR2_JEXTSEL_MASK)|ADC_CR2_JEXTSEL_JSWSTART|ADC_CR2_JEXTTRIG|ADC_CR2_ADON;
    for(unsigned k=0u;k<256u;k++){
        ADC_SR&=~ADC_SR_JEOC; ADC_CR2|=ADC_CR2_JSWSTART; uint32_t start=DWT_CYCCNT;
        while(!(ADC_SR&ADC_SR_JEOC)){if((uint32_t)(DWT_CYCCNT-start)>20000u){g_safety_latched=0xA001u;return;}}
        sum0+=(uint16_t)ADC_JDR1; sum1+=(uint16_t)ADC_JDR2; sum2+=(uint16_t)ADC_JDR3;
    }
    g_adc_offset[0]=(uint16_t)(sum0>>8); g_adc_offset[1]=(uint16_t)(sum1>>8); g_adc_offset[2]=(uint16_t)(sum2>>8);
    if (!g30_adc_offset_stock_zero_valid(g_adc_offset[0]) ||
        !g30_adc_offset_stock_zero_valid(g_adc_offset[1]) ||
        !g30_adc_offset_stock_zero_valid(g_adc_offset[2]))
        g_safety_latched=0xA012u;
    ADC_SR&=~ADC_SR_JEOC;
}

/* Both ADC2 current channels (CH4 and CH5) must be calibrated separately.
 * One-rank software injected conversions, with all six bridge GPIOs inactive.
 * ADC2 cannot inherit ADC1 CH5's offset: that created false current in four
 * PWM sectors even after v0.9.7 fixed ADC2 CH4 for the other two sectors. */
static void adc2_calibrate_injected_current_offsets(void) {
    g_adc2_ch4_offset_valid=0u;
    g_adc2_ch5_offset_valid=0u;
    ADC2_CR1=0u;
    ADC2_CR2=(ADC_CR2_ADON|ADC_CR2_JEXTSEL_JSWSTART|ADC_CR2_JEXTTRIG);
    for(uint8_t ch=4u;ch<=5u;ch++) {
        uint32_t sum=0u;
        ADC2_JSQR=((uint32_t)ch<<15); /* exactly ONE rank */
        for(unsigned i=0u;i<256u;i++) {
            ADC2_SR &= ~ADC_SR_JEOC;
            ADC2_CR2 |= ADC_CR2_JSWSTART;
            const uint32_t start=DWT_CYCCNT;
            while(!(ADC2_SR & ADC_SR_JEOC)) {
                if((uint32_t)(DWT_CYCCNT-start)>20000u) {
                    g_safety_latched=0xA011u;
                    return;
                }
            }
            const uint32_t value=ADC2_JDR1;
            if(value>4095u) { g_safety_latched=0xA011u;return; }
            sum+=value;
        }
        const uint16_t offset=(uint16_t)(sum>>8);
        if (!g30_adc_offset_stock_zero_valid(offset)) {
            g_safety_latched=0xA013u;
            return;
        }
        if(ch==4u) {g_adc2_ch4_offset=offset;g_adc2_ch4_offset_valid=1u;}
        else {g_adc2_ch5_offset=offset;g_adc2_ch5_offset_valid=1u;}
    }
    ADC2_SR &= ~ADC_SR_JEOC;
}

/* Boot calibration is deliberately single-ADC CH3/4/5, then BOTH ADCs
 * are switched into injected simultaneous mode. ADC1 CC4 is the master
 * trigger and ADC2 uses the SW trigger setting required for slave dual
 * mode. Identical sample times for CH3/4/5 are mandatory per RM0008.
 * ADC1 REGULAR channel PA1 measures VBUS separately and nonblocking.
 */
static void adc1_injected_init(void) {
    RCC_APB2ENR|=(1u<<9)|(1u<<10)|(1u<<2);
    gpio_cfg_nibble(GPIOA_BASE,1,0x0u);
    gpio_cfg_nibble(GPIOA_BASE,3,0x0u);
    gpio_cfg_nibble(GPIOA_BASE,4,0x0u);
    gpio_cfg_nibble(GPIOA_BASE,5,0x0u);
    /* Stock/XBOT common BEMF inputs: PA6=A, PA7=B, PB1=C. */
    RCC_APB2ENR|=(1u<<3);
    gpio_cfg_nibble(GPIOA_BASE,6,0x0u);gpio_cfg_nibble(GPIOA_BASE,7,0x0u);
    gpio_cfg_nibble(GPIOB_BASE,1,0x0u);
    const uint32_t sample=(5u<<(1*3))|(2u<<(3*3))|(2u<<(4*3))|(2u<<(5*3));
    ADC_CR1=ADC_CR1_SCAN;
    ADC2_CR1=0u;
    ADC_SMPR2=sample;
    ADC2_SMPR2=sample;
    /* Original boot calibration: four injected ranks CH3/4/5/1. */
    ADC_JSQR=(3u<<20)|(3u<<0)|(4u<<5)|(5u<<10)|(1u<<15);
    ADC_CR2=ADC_CR2_ADON;
    ADC2_CR2=ADC_CR2_ADON;
    for(volatile unsigned i=0u;i<1000u;i++)nop();
    ADC_CR2|=ADC_CR2_RSTCAL;while(ADC_CR2&ADC_CR2_RSTCAL){}
    ADC_CR2|=ADC_CR2_CAL;while(ADC_CR2&ADC_CR2_CAL){}
    ADC2_CR2|=ADC_CR2_RSTCAL;while(ADC2_CR2&ADC_CR2_RSTCAL){}
    ADC2_CR2|=ADC_CR2_CAL;while(ADC2_CR2&ADC_CR2_CAL){}
    adc1_calibrate_injected_offsets();
    if(!g_safety_latched) adc2_calibrate_injected_current_offsets();
    uint16_t offsets[3]={g_adc_offset[0],g_adc_offset[1],g_adc_offset[2]};
    stock_dual_adc_runtime_init(&g_dual_adc,offsets);
    g30_fixed_scan_init(&g_fixed_scan,offsets);
    /* One injected rank per ADC. ADC1 master drives the shared trigger.
       Single ADC1 regular conversion does not change the injected JSQR. */
    ADC_CR1=STOCK_ADC_MODE_REG_INJEC_SIMULT|ADC_CR1_SCAN|ADC_CR1_JEOCIE;
    ADC2_CR1=ADC_CR1_SCAN;
    ADC_JSQR=G30_FIXED_JSQR_A;
    ADC2_JSQR=G30_FIXED_JSQR_B;
    ADC2_CR2=ADC_CR2_ADON|ADC_CR2_JEXTSEL_JSWSTART|ADC_CR2_JEXTTRIG;
    ADC_SQR1=0u;ADC_SQR3=1u; /* single ADC1 REGULAR PA1 = VBUS */
    ADC_CR2=ADC_CR2_ADON|ADC_CR2_JEXTSEL_TIM1_CC4|ADC_CR2_JEXTTRIG|
            ADC_CR2_EXTSEL_SWSTART|ADC_CR2_EXTTRIG;
    ADC_SR&=~(ADC_SR_JEOC|ADC_SR_EOC);
    ADC2_SR&=~ADC_SR_JEOC;
    NVIC_IPR8(IRQ_ADC1_2)=0x20u;
    NVIC_ISER0=(1u<<IRQ_ADC1_2);
}

/* Execute from main loop, not inside current ISR: one REGULAR ADC1 conversion
 * every 20ms. JEOC injected sequence retains its own channel pair. */
static uint8_t passive_bemf_regular_start(void *ctx,uint8_t channel){
    (void)ctx;
    if(g_power_armed || g_vbus_pending || channel>17u)return 0u;
    ADC_SQR1=0u;ADC_SQR3=channel;ADC_SR&=~ADC_SR_EOC;ADC_CR2|=ADC_CR2_SWSTART;
    return 1u;
}
static uint8_t passive_bemf_regular_poll(void *ctx,uint16_t *value){
    (void)ctx;
    if(!(ADC_SR&ADC_SR_EOC))return 0u;
    if(value)*value=(uint16_t)ADC_DR;
    ADC_SR&=~ADC_SR_EOC;ADC_SQR3=1u;return 1u;
}
static void passive_bemf_regular_abort(void *ctx){
    (void)ctx;ADC_SR&=~ADC_SR_EOC;ADC_SQR3=1u;
}
static void passive_bemf_init(void){
    g30_rolling_bemf_config_t rc;g30_rolling_bemf_default_config(&rc);
#if ROLLING_SENSORLESS_RC_ALLOWED
#if DRIVE_RUNTIME_ALLOWED
    /* No donor motor geometry is silently treated as G30 calibration.
     * FF supplies measured pole pairs and circumference, in RAM. */
    rc.wheel_circ_mm=G30_PROFILE_CIRCUMFERENCE_MM;
    rc.pole_pairs=G30_PROFILE_POLE_PAIRS;
    rc.phase_offset_u16=(int16_t)G30_PROFILE_PHASE_OFFSET_U16;
    if(!G30_PROFILE_POLE_PAIRS || G30_PROFILE_POLE_PAIRS>64u ||
       G30_PROFILE_CIRCUMFERENCE_MM<300u || G30_PROFILE_CIRCUMFERENCE_MM>1800u) {
        rc.wheel_circ_mm=0u;rc.pole_pairs=0u;
    }
#else
    rc.wheel_circ_mm=763u;
    rc.pole_pairs=15u;
    rc.forward_raw_step=5u;
    rc.min_speed_kmh10=30u;
    rc.phase_offset_u16=(int16_t)FO70_FRAME_OFFSET_U16;
#endif
#endif
    g30_rolling_bemf_init(&g_rolling_bemf,&rc);
    const g30_passive_bemf_adc_io_t io={passive_bemf_regular_start,
        passive_bemf_regular_poll,passive_bemf_regular_abort};
    g30_passive_bemf_adc_init(&g_passive_bemf_adc,io,0);
}
static void passive_bemf_poll(void){
#if PASSIVE_BEMF_PROBE_ALLOWED
    const uint8_t blocked=(uint8_t)(g_power_armed || iap_update_active() || iap_update_pending());
    g30_passive_bemf_adc_tick(&g_passive_bemf_adc,g_ms,blocked,g_vbus_pending);
    uint16_t abc[3];
    if(g30_passive_bemf_adc_take(&g_passive_bemf_adc,abc))
        g30_rolling_bemf_sample(&g_rolling_bemf,abc[0],abc[1],abc[2],DWT_CYCCNT);
#else
    (void)g_passive_bemf_adc;
#endif
}

static void adc_vbus_poll(void){
    if(g_vbus_pending){
        if(ADC_SR&ADC_SR_EOC){
            g_adc_raw[3]=(uint16_t)ADC_DR;
            g_vbus_last_ms=g_ms;
            g_vbus_pending=0u;
            ADC_SR&=~ADC_SR_EOC;
        }else if((uint32_t)(g_ms-g_vbus_start_ms)>8u){
            g_vbus_pending=0u;
            if(g_vbus_timeout_events<65535u)g_vbus_timeout_events++;
        }
    }
    if(!g_vbus_pending && !g_passive_bemf_adc.pending && (uint32_t)(g_ms-g_vbus_start_ms)>=20u){
        ADC_SR&=~ADC_SR_EOC;
        g_vbus_start_ms=g_ms;
        g_vbus_pending=1u;
        ADC_CR2|=ADC_CR2_SWSTART;
    }
}

static void tim1_pwm_and_adc_trigger_init(void) {
    if (g_gate_kill_latched) return; /* never resurrect a killed MOE */
    RCC_APB2ENR|=(1u<<11); TIM_CR1(TIM1_BASE)=0u; TIM_CR2(TIM1_BASE)=0u; TIM_PSC(TIM1_BASE)=0u; TIM_ARR(TIM1_BASE)=PWM_ARR; TIM_RCR(TIM1_BASE)=1u;
    TIM_CCR1(TIM1_BASE)=PWM_ZERO_CCR; TIM_CCR2(TIM1_BASE)=PWM_ZERO_CCR; TIM_CCR3(TIM1_BASE)=PWM_ZERO_CCR; TIM_CCR4(TIM1_BASE)=ADC_TRIGGER_CCR;
    TIM_CCMR1(TIM1_BASE)=(6u<<4)|(1u<<3)|(6u<<12)|(1u<<11); TIM_CCMR2(TIM1_BASE)=(6u<<4)|(1u<<3)|(6u<<12)|(1u<<11);
    TIM_CCER(TIM1_BASE)=stock_tim1_gate_ccer_off(STOCK_TIM1_GATE_RUN_CCER);
    /* A prior LOCK=1 is immutable until MCU reset. Never blindly continue. */
    if (!tim1_bdtr_guard_preinit((uint16_t)TIM_BDTR(TIM1_BASE))) {
        g_safety_latched=0xA02Au;
        power_stage_force_disarm();
        return;
    }
    /* Passive ADC-only profile: AOE=0, BKE=0, BKP=0, LOCK=0,
       DTG unchanged and CH1/2/3 + CHxN disabled via CCER. */
    TIM_BDTR(TIM1_BASE)=DEADTIME_DTG|TIM_BDTR_OSSI|TIM_BDTR_OSSR|TIM_BDTR_MOE;
    if (!tim1_bdtr_guard_passive((uint16_t)TIM_BDTR(TIM1_BASE),
                                 (uint8_t)DEADTIME_DTG,
                                 (uint16_t)TIM_CCER(TIM1_BASE))) {
        g_safety_latched=0xA02Bu;
        power_stage_force_disarm();
        return;
    }
    TIM_EGR(TIM1_BASE)=TIM_EGR_UG; TIM_SR(TIM1_BASE)=0u; TIM_CR1(TIM1_BASE)=TIM_CR1_CMS_0|TIM_CR1_ARPE|TIM_CR1_CEN;
}


void Reset_Handler(void) {
    /* Do not depend on a specific stock-bootloader VTOR handoff. */
    SCB_VTOR=0x08001000u;
    __asm volatile("dsb" ::: "memory");
    __asm volatile("isb" ::: "memory");
    uint32_t *src=&_sidata; for(uint32_t *dst=&_sdata;dst<&_edata;)*dst++=*src++;
    for(uint32_t *dst=&_sbss;dst<&_ebss;)*dst++=0u; (void)main(); for(;;){}
}
void Default_Handler(void){g_safety_latched=0xDEAD0001u;power_stage_force_disarm();for(;;){}}
void SysTick_Handler(void) {
    g_ms++;
    motor_pipeline_watchdog(&g_motor_pipeline,g_ms,COMM_DASH_STALE_MS);
    /* Motor Detect vector commands are not allowed to remain energized if
     * the ADC/measurement loop stops issuing fresh requests. */
    if(g_id_bench_requested){
        if((uint32_t)(g_ms-g_id_bench_started_ms)>ID_BENCH_MAX_TOTAL_MS ||
           (g_power_armed && g_id_bench_energized_since_ms &&
            (uint32_t)(g_ms-g_id_bench_energized_since_ms)>ID_BENCH_MAX_ACTIVE_MS) ||
           g_dash_brake || !g_dash_seen ||
           g_dash_throttle>COMM_INPUT_IDLE_MAX ||
           (uint32_t)(g_ms-g_dash_last_ms)>COMM_DASH_STALE_MS){
            g_safety_latched=0xA021u;
            power_stage_force_disarm();
            motor_id_auto_abort(&g_motor_id_auto);
            g_id_bench_requested=0u;
        }
    }
    if(g_motor_id_vector_owner &&
       (uint32_t)(g_ms-g_motor_id_vector_last_ms)>12u){
        g_safety_latched=0xA022u;
        power_stage_force_disarm();
        motor_id_auto_abort(&g_motor_id_auto);
        g_id_bench_requested=0u;
    }
    const uint8_t button_pressed=(uint8_t)((GPIO_IDR(POWER_BUTTON_GPIO_BASE)&(1u<<POWER_BUTTON_PIN))!=0u);
    if(power_button_step(&g_power_button_state,button_pressed,(uint8_t)iap_update_active())) {
        motor_pipeline_stop(&g_motor_pipeline,MOTOR_STOP_DASH);
        g_poweroff_requested=1u;
        g_poweroff_deadline_ms=g_ms;
    }
}
/* IRQ25: hardware TIM1 update event has transferred CCR preloads into
 * active shadow registers. Priority above ADC IRQ means the ADC consumer
 * cannot accidentally observe an unpublished/old vector. */
void TIM1_UP_IRQHandler(void){
    if(TIM_SR(TIM1_BASE)&1u){
        TIM_SR(TIM1_BASE)&=~1u;
        if(g_motor_id_vector_owner && g_power_armed)
            motor_id_adc_feedback_on_update(&g_motor_id_feedback,DWT_CYCCNT);
    }
}
void USART2_IRQHandler(void){ninebot_link_irq();}

void ADC1_2_IRQHandler(void) {
    const uint32_t t0=DWT_CYCCNT;
    const uint32_t sr1=ADC_SR,sr2=ADC2_SR;
    if(!(sr1&ADC_SR_JEOC))return;
    const uint32_t cr1=ADC_CR1,cr2=ADC2_CR1;
    const uint32_t jsqr1=ADC_JSQR,jsqr2=ADC2_JSQR;
    const uint8_t modefault=g30_adc_mode_capture(&g_adc_mode_guard,
                                     cr1,cr2,ADC_CR2,sr2);
    stock_adc_timing_capture(&g_timer_evidence,
        (uint16_t)TIM_CNT(TIM1_BASE),(uint16_t)TIM_CCR4(TIM1_BASE),
        (uint16_t)TIM_ARR(TIM1_BASE),(uint16_t)TIM_CR1(TIM1_BASE),
        (uint16_t)TIM_CCER(TIM1_BASE),(uint16_t)TIM_BDTR(TIM1_BASE),
        (uint16_t)TIM_SR(TIM1_BASE),t0);
    if (g_power_armed && g_motor_id_vector_owner) {
        /* One physical ADC1/ADC2 injected pair, already in the owned
         * stock sector. No blind use of requested mV or CCR preloads. */
        const uint16_t raw1=(uint16_t)ADC_JDR1,raw2=(uint16_t)ADC2_JDR1;
        /* Per-injected-sample hard trip independent of any mA conversion.
         * Reject either analogue rail and any channel offset excursion. */
        const uint16_t ref1=(uint16_t)(jsqr1>>15)&31u;
        const uint16_t ref2=(uint16_t)(jsqr2>>15)&31u;
        const uint16_t off1=(ref1==3u)?g_adc_offset[0]:(ref1==4u)?g_adc_offset[1]:g_adc_offset[2];
        const uint16_t off2=(ref2==4u)?g_adc2_ch4_offset:g_adc2_ch5_offset;
        if(!g_offset_cal_valid || raw1>4095u || raw2>4095u ||
           ref1<3u || ref1>5u || ref2<4u || ref2>5u ||
           abs16s((int32_t)raw1-off1)>ID_BENCH_RAW_TRIP_COUNTS ||
           abs16s((int32_t)raw2-off2)>ID_BENCH_RAW_TRIP_COUNTS) {
            ADC_SR&=~ADC_SR_JEOC;ADC2_SR&=~ADC_SR_JEOC;
            ++g_overcurrent_trips;
            g_safety_latched=0xA020u;
            power_stage_force_disarm();
            return;
        }
        const motor_id_adc_input_t in={
            .now_cycles=t0,.clock_hz=SYSCLK_HZ,
            .vbus_mv=(uint32_t)g_adc_raw[3]*14u,
            .vbus_age_ms=g_ms-g_vbus_last_ms,
            .jsqr1=jsqr1,.jsqr2=jsqr2,
            .adc1_raw=raw1,.adc2_raw=raw2,
            .offset={g_adc_offset[0],g_adc_offset[1],g_adc_offset[2]},
            .adc2_ch4_offset=g_adc2_ch4_offset,
            .adc2_ch5_offset=g_adc2_ch5_offset,
            .tim1_arr=PWM_ARR,
            .adc1_jeoc=(uint8_t)!!(sr1&ADC_SR_JEOC),
            .adc2_jeoc=(uint8_t)!!(sr2&ADC_SR_JEOC),
            .dual_mode_valid=(uint8_t)(
                (cr1&STOCK_ADC_MODE_REG_INJEC_SIMULT)==STOCK_ADC_MODE_REG_INJEC_SIMULT),
            .window_valid=(uint8_t)(
                (g_timer_evidence.config_flags&~ADC_TIMING_GATE_ENABLED)==0u),
            .adc2_ch4_offset_valid=g_adc2_ch4_offset_valid,
            .adc2_ch5_offset_valid=g_adc2_ch5_offset_valid,
            .current_scale_qualified=(uint8_t)(g_offset_cal_valid &&
                (COMM_CURRENT_SCALE_HW_VALID || (MOTOR_ID_BENCH_ALLOWED && g_id_bench_requested))),
            .timing_qualified=(uint8_t)(COMM_ADC_TIMING_HW_VALID ||
                (MOTOR_ID_BENCH_ALLOWED && g_id_bench_requested))
        };
        ADC_SR&=~ADC_SR_JEOC;ADC2_SR&=~ADC_SR_JEOC;
        motor_id_auto_feedback_t fb={0};
        const uint8_t valid=motor_id_adc_feedback_capture(
            &g_motor_id_feedback,&in,&fb);
        g_adc_samples++;
        g_adc_last_ms=g_ms;
        if(valid){
            g_motor_id_vector_last_ms=g_ms; /* real feedback keeps watchdog alive */
            if(fb.command_epoch==g_motor_id_auto.command_epoch){
                if(g_motor_id_auto.state==ID_AUTO_FLUX_SPIN){
                    motor_id_flux_feedback_observe(&g_motor_id_flux_feedback,
                        &g_motor_id_auto.id,t0/(SYSCLK_HZ/1000000u),&fb);
                }
                fb.throttle_idle=(uint8_t)(g_dash_throttle<=COMM_INPUT_IDLE_MAX);
                fb.brake_active=g_dash_brake;
                fb.fault_active=(uint8_t)(g_safety_latched || g_poweroff_requested);
                fb.stop_requested=(uint8_t)(iap_update_active() || iap_update_pending());
                if(!motor_id_auto_tick(&g_motor_id_auto,t0/(SYSCLK_HZ/1000000u),&fb)){
                    g_safety_latched=0xA01Au;
                    power_stage_force_disarm();
                }
            }
        }else if(!g_motor_id_feedback.pending &&
                 g_motor_id_feedback.active_valid){
            /* Bad/misaligned actual sample with no in-flight preload. */
            g_safety_latched=0xA019u;
            power_stage_force_disarm();
        }
        return;
    }
    if (g_power_armed) {
        /* ACTIVE-FIRMWARE ROUTE (compiled but deliberately unreachable in
         * GATE-OFF builds). The same event supplies both ADC injected JDR1s.
         * No fixed two-rank diagnostic value is permitted here. */
        foc_adc_sample_t sample={0};
        sample.sequence=++g_motor_irq_sequence;
        sample.sector=g_motor_pipeline.adc.expected_sector;
        sample.adc1_jsqr=jsqr1; sample.adc2_jsqr=jsqr2;
        sample.adc1_sr=(uint16_t)sr1;sample.adc2_sr=(uint16_t)sr2;
        sample.adc1_jdr1=(uint16_t)ADC_JDR1;
        sample.adc2_jdr1=(uint16_t)ADC2_JDR1;
        /* ADC2 CH4 and CH5 each have an independent zero-current offset.
         * Both are normalized to the ADC1-based reconstruction coordinate;
         * a missing/invalid channel calibration invalidates this entire pair. */
        const uint16_t reference_offsets[3]={g_adc_offset[0],g_adc_offset[1],g_adc_offset[2]};
        const uint8_t offset_pair_ok=g30_adc2_current_offset_normalize(
             sample.sector,sample.adc2_jdr1,reference_offsets,
             g_adc2_ch4_offset,g_adc2_ch5_offset,
             g_adc2_ch4_offset_valid,g_adc2_ch5_offset_valid,
             &sample.adc2_jdr1);
        sample.dual_injected_simultaneous=(uint8_t)(
            (cr1&STOCK_ADC_MODE_REG_INJEC_SIMULT)==STOCK_ADC_MODE_REG_INJEC_SIMULT);
        /* Model validity is checked against THE SAME PWM period's sector,
         * both injected ADC settings and the three CCR duty boundaries.
         * COMM_ADC_TIMING_HW_VALID remains OFF until analogue qualification. */
        const g30_adc_current_state_t current_adc_state={
            .rcc_cfgr=RCC_CFGR,
            .adc1_cr1=cr1,.adc2_cr1=cr2,
            .adc1_cr2=ADC_CR2,.adc2_cr2=ADC2_CR2,
            .adc1_smpr2=ADC_SMPR2,.adc2_smpr2=ADC2_SMPR2,
            .tim1_cr1=(uint16_t)TIM_CR1(TIM1_BASE),
            .tim1_ccer=(uint16_t)TIM_CCER(TIM1_BASE),
            .tim1_ccmr2=(uint16_t)TIM_CCMR2(TIM1_BASE),
            .tim1_psc=(uint16_t)TIM_PSC(TIM1_BASE),
            .tim1_rcr=(uint16_t)TIM_RCR(TIM1_BASE),
            .tim1_arr=(uint16_t)TIM_ARR(TIM1_BASE),
            .tim1_ccr4=(uint16_t)TIM_CCR4(TIM1_BASE),
            .tim1_ccr1=(uint16_t)TIM_CCR1(TIM1_BASE),
            .tim1_ccr2=(uint16_t)TIM_CCR2(TIM1_BASE),
            .tim1_ccr3=(uint16_t)TIM_CCR3(TIM1_BASE),
            .sector=sample.sector
        };
        sample.sample_window_valid=(uint8_t)(
            (COMM_ADC_TIMING_HW_VALID || ROLLING_SENSORLESS_RC_ALLOWED) &&
            offset_pair_ok && g30_adc_current_flags(&current_adc_state)==0u);
        ADC_SR&=~ADC_SR_JEOC; ADC2_SR&=~ADC_SR_JEOC;
        g_adc_last_ms=g_ms;
        if ((uint32_t)(g_ms-g_vbus_last_ms)>100u) {
            if (!g_safety_latched)g_safety_latched=0xA00Au;
            motor_pipeline_stop(&g_motor_pipeline,MOTOR_STOP_ADC);
        } else if (!motor_pipeline_on_adc(&g_motor_pipeline,&sample,g_adc_raw[3])) {
            if (!g_safety_latched)
                g_safety_latched=(g_motor_pipeline.last_stop_reason==MOTOR_STOP_FAULT)?
                                    0xA00Bu:0xA00Au;
            /* The common stop path has ALREADY removed hardware gates. */
        } else {
            const stock_phase_currents_t z=g_motor_pipeline.last_phase;
            uint16_t peak=abs16s(z.ia),v=abs16s(z.ib);
            if (v>peak)peak=v;v=abs16s(z.ic);if(v>peak)peak=v;
            g_last_abs_current=peak;
            if (peak>g_peak_abs_current)g_peak_abs_current=peak;
            if (peak>HARD_OC_COUNTS) {
                g_overcurrent_trips++;
                g_safety_latched=0xA00Bu;
                motor_pipeline_stop(&g_motor_pipeline,MOTOR_STOP_FAULT);
            }
            if ((g_motor_irq_sequence&1u)==0u)g_control_ticks++;
        }
        g_adc_samples++;
        const uint32_t cost=DWT_CYCCNT-t0;
        g_last_isr_cycles=cost;
        if (cost>g_max_isr_cycles)g_max_isr_cycles=cost;
        if (cost>CONTROL_HARD_CYCLES) {
            g_control_hard_overruns++;
            g_safety_latched=0xA00Cu;
            motor_pipeline_stop(&g_motor_pipeline,MOTOR_STOP_FAULT);
        }
        return;
    }
    if(g_timer_evidence.config_flags&ADC_TIMING_GATE_ENABLED){
        if(!g_safety_latched)g_safety_latched=0xA006u;
        power_stage_force_disarm();
    }
    /* Two ranks are converted per ADC after ONE injected timer trigger.
       CH3 (A) and CH5 (C) share rank 1; CH4 (B) and CH5 again share
       rank 2. Rank 2 is deliberately NOT simultaneous with rank 1. */
    const uint16_t a=(uint16_t)ADC_JDR1,b=(uint16_t)ADC_JDR2;
    /* ADC2 JDRx must NEVER be treated as fresh without ADC2 JEOC. */
    const uint16_t c=(modefault&G30_ADCMODE_ADC2_NOT_READY)?0xffffu:(uint16_t)ADC2_JDR1;
    const uint16_t c2=(modefault&G30_ADCMODE_ADC2_NOT_READY)?0xffffu:(uint16_t)ADC2_JDR2;
    ADC_SR&=~ADC_SR_JEOC;ADC2_SR&=~ADC_SR_JEOC;
    const uint8_t q=g30_fixed_scan_capture(&g_fixed_scan,jsqr1,jsqr2,
                           cr1,cr2,(uint16_t)sr1,(uint16_t)sr2,a,b,c,c2);
    /* In this build these are passive observations, NOT FOC phase
       currents qualified for motor torque, nor a SVM sector. */
    stock_adc_snapshot_capture(&g_pair_snapshot,0u,a,c,jsqr1,jsqr2,
                                (uint16_t)sr1,(uint16_t)sr2,g_ms);
    stock_adc_correlated_capture(&g_correlated_evidence,&g_pair_snapshot,
            &g_timer_evidence,(q&G30_SCAN_FATAL_MASK)?ADCQ_RAW_INVALID:0u);
    g_last_adc1_pair=a;g_last_adc2_pair=c;g_last_pair_sector=0u;
    if(g_adc_samples){
        const uint32_t dt=t0-g_last_sample_cycle;
        if(dt<g_sample_interval_min)g_sample_interval_min=dt;
        if(dt>g_sample_interval_max)g_sample_interval_max=dt;
    }
    g_last_sample_cycle=t0;g_adc_last_ms=g_ms;
    g_adc_quality.total_checked++;
    g_adc_quality.last_flags=(q&G30_SCAN_FATAL_MASK)?ADCQ_RAW_INVALID:0u;
    if(q&G30_SCAN_FATAL_MASK){
        /* Preserve the meaning of diagnostic counters: offset-noise
         * rejection must NOT be misreported as a corrupt ADC raw value. */
        if((q&G30_SCAN_BAD_RAW)&&g_adc_quality.invalid_raw_events!=65535u)
            g_adc_quality.invalid_raw_events++;
        if((q&G30_SCAN_BAD_JSQR)&&g_adc_quality.wrong_channel_events!=65535u)
            g_adc_quality.wrong_channel_events++;
        g_adc_quality.fault_latched=1u;
        if(!g_safety_latched)g_safety_latched=(modefault&G30_ADCMODE_ADC2_NOT_READY)?
           0xA007u:modefault?0xA008u:(q&G30_SCAN_BAD_JSQR)?0xA009u:0xA004u;
        power_stage_force_disarm();
        g_offset_cal_active=0u;g_offset_cal_valid=0u;
        g_adc_samples++;
        return;
    }
    for(unsigned i=0u;i<3u;i++)g_adc_raw[i]=g_fixed_scan.raw[i];
    /* Real coast/rest feedback after physical gate isolation uses all three
     * channels from the existing passive injected diagnostic scan. No
     * fictitious current or PWM voltage is injected. Flux BEMF/speed are
     * deliberately NOT supplied (separate point 2.3). */
    if(g_motor_id_auto.state==ID_AUTO_L_REST){
        if(g_offset_cal_valid &&
           ((COMM_CURRENT_SCALE_HW_VALID && COMM_ADC_TIMING_HW_VALID) ||
            (MOTOR_ID_BENCH_ALLOWED && g_id_bench_requested)) && !g_power_armed &&
           g_fixed_scan.offset_valid &&
           (uint32_t)(g_ms-g_vbus_last_ms)<=100u){
            motor_id_auto_feedback_t fb={0};
            fb.command_epoch=g_motor_id_auto.command_epoch;
            fb.adc_valid=1u;fb.voltage_valid=1u;
            fb.applied_voltage_mv=0;
            /* The six-vector inductance test changes which winding is
             * excited. Verify that ALL three physical shunt currents have
             * decayed before energizing the next vector. Checking phase A
             * alone would miss residual current in phase B or C. */
            uint32_t rest_peak_counts=0u;
            for(unsigned ph=0;ph<3u;ph++){
                const int32_t diff=(int32_t)g_fixed_scan.raw[ph]-(int32_t)g_adc_offset[ph];
                const uint32_t abs_counts=(uint32_t)(diff<0?-diff:diff);
                if(abs_counts>rest_peak_counts)rest_peak_counts=abs_counts;
            }
            fb.phase_current_ma=stock_current_nominal_counts_to_ma((int32_t)rest_peak_counts);
            fb.throttle_idle=(uint8_t)(g_dash_throttle<=COMM_INPUT_IDLE_MAX);
            fb.brake_active=g_dash_brake;
            fb.fault_active=(uint8_t)(g_safety_latched || g_poweroff_requested);
            fb.stop_requested=(uint8_t)(iap_update_active() || iap_update_pending());
            if(!motor_id_auto_tick(&g_motor_id_auto,t0/(SYSCLK_HZ/1000000u),&fb)){
                g_safety_latched=0xA01Au;
                power_stage_force_disarm();
            }
        }
    }
    if(g_offset_cal_active && g_fixed_scan.offset_valid){
        for(unsigned i=0u;i<3u;i++)g_adc_offset[i]=g_fixed_scan.offsets[i];
        g_offset_cal_active=0u;g_offset_cal_valid=1u;
        g_last_action_status=ACT_OK;
    }
    /* Uncalibrated mapping/sign! Never feed these measurements into an
       enabled power stage without separate physical validation. */
    const int32_t ia=(int32_t)a-g_adc_offset[0];
    const int32_t ib=(int32_t)b-g_adc_offset[1];
    const int32_t ic=(int32_t)c-g_adc_offset[2];
    uint16_t peak=abs16s(ia),z=abs16s(ib);
    if(z>peak)peak=z;z=abs16s(ic);if(z>peak)peak=z;
    g_last_abs_current=peak;
    if(peak>g_peak_abs_current)g_peak_abs_current=peak;
    if(++g_control_div4>=2u){
        g_control_div4=0u;
        phase_current_counts_t current={(int16_t)ia,(int16_t)ib,(int16_t)ic};
        const uint32_t c0=DWT_CYCCNT;
        /* Do not even feed invalid or uncalibrated current estimates to
         * the sensorless state machine. Physical outputs stay disabled. */
        if(g_offset_cal_valid && !g_safety_latched)
            sensorless_control_step(&ctrl,current,g_adc_raw[3],PWM_ARR,0u);
        else
            sensorless_control_stop(&ctrl);
        const uint32_t used=DWT_CYCCNT-c0;
        g_last_control_cycles=used;
        if(used>g_max_control_cycles)g_max_control_cycles=used;
        if(used>CONTROL_HARD_CYCLES)g_control_hard_overruns++;
        g_control_ticks++;
    }
    /* NO JSQR WRITES HERE. Channel selection is frozen at boot. */
    motor_probe_capture(g_adc_raw[0],g_adc_raw[1],g_adc_raw[2],g_adc_raw[3],
                        g_adc_offset,g_ms,g_sample_interval_min,g_sample_interval_max,
                        g_max_control_cycles,0u,jsqr1,ADC_CR2,
                        TIM_CCER(TIM1_BASE),TIM_BDTR(TIM1_BASE));
    g_adc_samples++;
    if((g_adc_samples&0x3Fu)==0u)(void)gate_safety_check();
    const uint32_t cycles=DWT_CYCCNT-t0;
    g_last_isr_cycles=cycles;
    if(cycles>g_max_isr_cycles)g_max_isr_cycles=cycles;
}

static void put_u16(uint8_t *p,uint16_t v){p[0]=(uint8_t)v;p[1]=(uint8_t)(v>>8);} 
static void put_i16(uint8_t *p,int16_t v){put_u16(p,(uint16_t)v);} 
static void put_u32(uint8_t *p,uint32_t v){put_u16(p,(uint16_t)v);put_u16(p+2,(uint16_t)(v>>16));}
static uint16_t get_u16(const uint8_t *p){return (uint16_t)p[0]|((uint16_t)p[1]<<8);} 
static int16_t get_i16(const uint8_t *p){return (int16_t)get_u16(p);} 
static uint32_t get_u32(const uint8_t *p){return (uint32_t)get_u16(p)|((uint32_t)get_u16(p+2)<<16);} 
static uint16_t current_fault_code(void){if(g_safety_latched)return (uint16_t)g_safety_latched;return (uint16_t)ctrl.fault;}

static uint16_t commissioning_guard_mask(void){
    commissioning_guard_input_t in;
    in.now_ms=g_ms;in.adc_last_ms=g_adc_last_ms;in.dash_last_ms=g_dash_last_ms;
    in.adc_stale_ms=ADC_STALE_MS;in.dash_stale_ms=COMM_DASH_STALE_MS;
    in.abs_current_counts=g_last_abs_current;in.idle_current_limit_counts=ARM_IDLE_CURRENT_COUNTS;
    in.vbus_raw=((uint32_t)(g_ms-g_vbus_last_ms)<=100u)?g_adc_raw[3]:0u;in.vbus_raw_min=50u;in.vbus_raw_max=4050u;
    in.offset_valid=g_offset_cal_valid;in.params_valid=ctrl.motor_params_valid;in.dash_seen=g_dash_seen;
    in.throttle=g_dash_throttle;in.brake=g_dash_brake;in.input_idle_max=COMM_INPUT_IDLE_MAX;
    in.safety_latched=(uint8_t)(g_safety_latched!=0u);
    in.iap_busy=(uint8_t)(iap_update_pending()||iap_update_active());
    in.poweroff_pending=g_poweroff_requested;
    in.control_stopped=(uint8_t)(ctrl.state==SENSORLESS_STOP && !ctrl.drive_request && !g_power_armed);
    in.current_scale_hw_valid=COMM_CURRENT_SCALE_HW_VALID;
    in.gate_hw_valid=COMM_GATE_STOCK_PROFILE_READY;
    in.adc_timing_hw_valid=COMM_ADC_TIMING_HW_VALID;
    return commissioning_guard_eval(&in);
}

static uint8_t status_flags(void){
    uint8_t f=0u;
#if POWER_STAGE_ARM_ALLOWED
    f|=0x01u;
#endif
#if SENSORLESS_RUN_ALLOWED
    f|=0x02u;
#endif
    if(g_power_armed)f|=0x04u;if(ctrl.observer_valid)f|=0x08u;if(g_offset_cal_active)f|=0x10u;if(g_dash_seen)f|=0x20u;if(g_cfg_dirty)f|=0x40u;
    f|=0x80u; /* provisional R/L/flux observer scaling */
    return f;
}

static uint16_t diag_flags(void){
    uint16_t f=0u;if(g_overcurrent_trips)f|=1u;if(g_control_hard_overruns)f|=2u;if((uint32_t)(g_ms-g_adc_last_ms)>ADC_STALE_MS)f|=4u;
    if(g_adc_quality.fault_latched)f|=512u;
    if(ninebot_link_rx_overflows())f|=8u;if(g_last_action_status==ACT_UNSUPPORTED)f|=16u;if(g_cfg_dirty)f|=32u;if(!g_phase_map_valid)f|=64u;f|=128u;if(g_shu_handoff_refused)f|=256u;return f;
}

static const uint8_t g_esc_identity[14]={'D','E','L','T','A','E','S','C','-','G','3','0','D','0'};

static void app_read_response(const ninebot_frame_t *req){
    uint8_t cmd=req->arg;uint8_t requested=req->payload_len?req->payload[0]:0u;
    uint8_t p[32];uint8_t n=0u;
    for(uint8_t i=0u;i<sizeof p;i++)p[i]=0u;
    switch(cmd){
    case 0x10u: for(uint8_t i=0u;i<14u;i++)p[i]=g_esc_identity[i];n=14u;break;
    case 0x1Au: case 0x28u: case 0x66u: case 0x67u: case 0x68u: put_u16(p,STOCK_COMPAT_FW_WORD);n=2u;break;
    case 0x79u: put_u16(p,0u);n=2u;break;
    case 0xB0u: put_u16(p+8,50u);put_u16(p+10,0u);put_i16(p+22,0);n=32u;break;
    case 0xD0u: p[0]='D';p[1]='E';p[2]='S';p[3]='C';p[4]=PROTO_MAJOR;p[5]=PROTO_MINOR;put_u16(p+6,FW_BUILD);p[8]=ctrl.state;p[9]=status_flags();put_u16(p+10,current_fault_code());put_u16(p+12,PWM_HZ);put_u16(p+14,CONTROL_HZ);n=16u;break;
    case 0xD1u: put_u16(p,g_adc_offset[0]);put_u16(p+2,g_adc_offset[1]);put_u16(p+4,g_adc_offset[2]);put_u16(p+6,g_adc_raw[0]);put_u16(p+8,g_adc_raw[1]);put_u16(p+10,g_adc_raw[2]);put_u16(p+12,g_adc_raw[3]);n=14u;break;
    case 0xD2u: p[0]=g_phase_map[0];p[1]=g_phase_map[1];p[2]=g_phase_map[2];p[3]=g_phase_signs;p[4]=g_phase_direction;n=5u;break;
    case 0xD3u: put_u32(p,g_cfg_r_uohm);n=4u;break;
    case 0xD4u: put_u32(p,g_cfg_l_nh);n=4u;break;
    case 0xD5u: put_u32(p,g_cfg_flux_uwb);n=4u;break;
    case 0xD6u: put_u16(p,ctrl.control_phase);put_u16(p+2,ctrl.observer_phase);put_i16(p+4,ctrl.phase_error);put_i16(p+6,(int16_t)sensorless_counts_to_ma(ctrl.iq));put_i16(p+8,(int16_t)sensorless_counts_to_ma(ctrl.id));put_u32(p+10,ctrl.bemf_mv);n=14u;break;
    case 0xD7u:{uint16_t conf=0u;if(ctrl.observer_valid){int32_t e=ctrl.phase_error;if(e<0)e=-e;if(e>5461)e=5461;conf=(uint16_t)(1000-(e*1000)/5461);}p[0]=ctrl.state;p[1]=ctrl.observer_valid;put_u16(p+2,ctrl.lock_ticks);put_u16(p+4,ctrl.lost_ticks);put_u16(p+6,conf);n=8u;break;}
    case 0xD8u: put_u32(p,g_last_control_cycles);put_u32(p+4,g_max_control_cycles);put_u32(p+8,CONTROL_BUDGET_CYCLES);n=12u;break;
    case 0xD9u:
        put_u16(p,current_fault_code());put_u16(p+2,diag_flags());put_u16(p+4,HARD_OC_COUNTS);
        put_u16(p+6,commissioning_guard_mask());p[8]=g_offset_cal_valid;p[9]=ctrl.motor_params_valid;
        p[10]=g_phase_map_valid;p[11]=(uint8_t)(g_last_action_status |
        (COMM_STOCK_CURRENT_MODEL_VALID ? 0x80u : 0u) |
        (COMM_STOCK_DUAL_ADC_PLAN_VALID ? 0x40u : 0u));put_u16(p+12,g_adc_raw[3]);put_u16(p+14,g_cfg_test_current_ma);n=16u;break;
    case 0xE8u: /* READ-only, self-consistent current sample pair. */
        /* Prevent interrupt tearing: E8 retains the v0.3.3 phone ABI. */
        irq_disable();
        put_u16(p,g_last_adc1_pair);
        put_u16(p+2,g_last_adc2_pair);
        put_u16(p+4,g_adc_raw[3]);
        put_u16(p+6,(uint16_t)(g_ms-g_vbus_last_ms));
        p[8]=g_last_pair_sector;
        {
            p[9]=3u;p[10]=5u; /* fixed rank 1; sector=0 (not SVM) */
        }
        p[11]=(uint8_t)((g_offset_cal_active?1u:0u)|
                       (g_offset_cal_valid?2u:0u)|
                       (g_vbus_pending?4u:0u)|
                       (g_adc_quality.fault_latched?8u:0u)|
                       ((g_adc_quality.last_flags&ADCQ_GAP_LONG)?16u:0u)|
                       ((g_adc_quality.last_flags&ADCQ_INTERVAL_SHORT)?32u:0u));
        put_u16(p+12,g_vbus_timeout_events);
        put_u16(p+14,(uint16_t)(g_pair_snapshot.adc2_jsqr&0xffffu));
        irq_enable();
        n=16u;break;
    case 0xEBu: /* register evidence, not physical sample time */
        irq_disable();
        stock_adc_timing_encode_eb(&g_timer_evidence,p);
        irq_enable();
        n=16u;break;
    case 0xF6u: /* read-only offset calibration min/max and CH5 duplicate spread */
        irq_disable();
        g30_fixed_scan_encode_f6(&g_fixed_scan,p);
        irq_enable();
        n=16u;break;
    case 0xEFu: /* fixed two-rank raw triple, offsets, duplicate CH5 */
        irq_disable();
        g30_fixed_scan_encode_ef(&g_fixed_scan,p);
        irq_enable();
        n=16u;break;
    case 0xEEu: /* read-only ADC operating mode and ADC2 JEOC qualification */
        irq_disable();
        g30_adc_mode_encode_ee(&g_adc_mode_guard,p);
        irq_enable();
        n=16u;break;
    case 0xEDu: /* one 16-byte READ-ONLY ADC+TIM1 sample, versioned ABI */
        irq_disable();
        stock_adc_correlated_encode_ed(&g_correlated_evidence,p);
        irq_enable();
        n=16u;break;
    case 0xECu: /* IRQ interval and timer config checks (diagnostic only) */
        irq_disable();
        stock_adc_timing_encode_ec(&g_timer_evidence,p);
        irq_enable();
        n=16u;break;
    case 0xEAu: /* full JSQR and JEOC state of one completed ADC sample pair */
        irq_disable();
        stock_adc_snapshot_encode_ea(&g_pair_snapshot,p);
        irq_enable();
        n=16u;break;
    case 0xE9u: /* read-only acquisition fault statistics */
        irq_disable();
        put_u32(p,g_adc_quality.total_checked);
        put_u16(p+4,g_adc_quality.invalid_raw_events);
        put_u16(p+6,g_adc_quality.wrong_channel_events);
        put_u16(p+8,g_adc_quality.long_gap_events);
        put_u16(p+10,g_adc_quality.short_gap_events);
        p[12]=g_adc_quality.last_flags;
        p[13]=g_adc_quality.fault_latched;
        put_u16(p+14,(uint16_t)g_safety_latched);
        irq_enable();
        n=16u;break;
    case 0xE7u: /* read-only status of staged motor configuration */
        p[0]=g_motor_cfg.pending_mask;
        p[1]=g_motor_cfg.active_valid;
        p[2]=ctrl.motor_params_valid;
        p[3]=g_power_armed;
        put_u16(p+4,g_cfg_test_current_ma);
        put_u16(p+6,commissioning_guard_mask());
        put_u32(p+8,g_cfg_r_uohm);
        put_u32(p+12,g_cfg_l_nh);
        n=16u;break;
    case 0xF7u: /* v0.9.3 read-only Motor-ID stage/result; never starts test */
        p[0]=g_motor_id_auto.id.state;
        p[1]=g_motor_id_auto.id.fault;
        p[2]=g_motor_id_auto.id.result.valid;
        p[3]=0u; /* external power-stage qualification NOT granted */
        put_u32(p+4,g_motor_id_auto.id.result.r_uohm);
        put_u32(p+8,g_motor_id_auto.id.result.l_nh);
        put_u32(p+12,g_motor_id_auto.id.result.flux_uwb);
        n=16u;break;
    case 0xF8u: /* Automatic motor ID sequencer status, read only. */
        p[0]=g_motor_id_auto.state;p[1]=g_motor_id_auto.fault;
        p[2]=g_motor_id_auto.output_active;p[3]=g_motor_id_auto.id.state;
        put_u32(p+4,g_motor_id_auto.seq);
        put_u32(p+8,g_motor_id_auto.id.result.samples_r);
        put_u32(p+12,g_motor_id_auto.id.result.samples_l);n=16u;break;
    case 0xF9u:
        p[0]=g_id_bench_requested;p[1]=g_motor_id_vector_owner;
        p[2]=g_power_armed;p[3]=g_motor_id_auto.state;
        put_u16(p+4,(uint16_t)g_safety_latched);
        put_u16(p+6,(uint16_t)g_last_abs_current);
        put_u16(p+8,(uint16_t)g_peak_abs_current);
        put_u32(p+10,g_motor_id_feedback.samples_accepted);
        put_u32(p+14,g_motor_id_feedback.samples_rejected);
        n=18u;break;
    case 0xFAu: case 0xFBu: { /* read-only ADC1/2 raw-and-offset evidence */
        /* Immutable snapshot from one IRQ-critical section; never read
         * half-old / half-new ADC offsets in one Ninebot BLE reply. */
        g30_adc_proof_input_t x={0};
        uint16_t bdtr,vbus_raw,ilast,ipeak,fault;
        uint32_t primask_before;
        __asm volatile("mrs %0, primask" : "=r"(primask_before) :: "memory");
        irq_disable();
        for(unsigned i=0u;i<3u;i++){
            x.raw[i]=g_fixed_scan.raw[i];
            x.off[i]=g_adc_offset[i];
        }
        x.adc2_ch4_off=g_adc2_ch4_offset;
        x.adc2_ch5_off=g_adc2_ch5_offset;
        x.ccer=(uint16_t)TIM_CCER(TIM1_BASE);
        bdtr=(uint16_t)TIM_BDTR(TIM1_BASE);
        x.have_sample=(uint8_t)(g_adc_samples>0u);
        x.adc1_offsets_valid=(uint8_t)(g_offset_cal_valid &&
                                            g_fixed_scan.offset_valid);
        x.adc2_ch4_valid=g_adc2_ch4_offset_valid;
        x.adc2_ch5_valid=g_adc2_ch5_offset_valid;
        x.killed=g_gate_kill_latched;x.powered=g_power_armed;
        x.now_ms=g_ms;x.last_adc_ms=g_adc_last_ms;
        vbus_raw=g_adc_raw[3];
        ilast=(uint16_t)g_last_abs_current;
        ipeak=(uint16_t)g_peak_abs_current;
        fault=(uint16_t)g_safety_latched;
        if(!(primask_before&1u))irq_enable();
        const g30_adc_proof_result_t o=g30_adc_proof_eval(&x);
        if(cmd==0xFAu)g30_adc_proof_page_fa(p,&x,o,fault);
        else g30_adc_proof_page_fb(p,&x,o,bdtr,vbus_raw,ilast,ipeak);
        n=18u;break;
    }
    case 0xFFu: /* measured motor geometry, readback; existing F6 ADC page unchanged */
        p[0]='P';p[1]='F';p[2]='1';p[3]='3';
        p[4]=g_rolling_bemf.cfg.pole_pairs;p[5]=g_rolling_bemf.cfg.forward_raw_step;
        put_u16(p+6,g_rolling_bemf.cfg.wheel_circ_mm);
        put_i16(p+8,g_rolling_bemf.cfg.phase_offset_u16);
        p[10]=ctrl.motor_params_valid;p[11]=DRIVE_RUNTIME_ALLOWED;n=12u;break;
    case 0xFCu: { /* read-only FO70-derived rolling BEMF evidence */
        uint32_t now=DWT_CYCCNT;
        uint32_t age_cycles=g_rolling_bemf.last_edge_cycles?
            (uint32_t)(now-g_rolling_bemf.last_edge_cycles):0xffffffffu;
        uint16_t age_ms=(uint16_t)(age_cycles==0xffffffffu?65535u:
            (age_cycles/(SYSCLK_HZ/1000u)>65535u?65535u:age_cycles/(SYSCLK_HZ/1000u)));
        p[0]='R';p[1]='K';p[2]='1';p[3]='0';
        p[4]=(uint8_t)((g30_rolling_bemf_ready(&g_rolling_bemf,now)?1u:0u)|
             (g_rolling_bemf.direction_forward?2u:0u)|
             (g_passive_bemf_adc.fresh?4u:0u));
        p[5]=g_rolling_bemf.last_sector;
        put_u16(p+6,g30_rolling_bemf_speed_kmh10(&g_rolling_bemf,now));
        put_u16(p+8,g_rolling_bemf.span);put_u16(p+10,g_rolling_bemf.forward_edges);
        put_u16(p+12,g_rolling_bemf.reverse_edges);put_u16(p+14,g_rolling_bemf.bad_jumps);
        put_u16(p+16,g_rolling_bemf.phase_u16);put_u16(p+18,(uint16_t)g_rolling_bemf.electrical_step_u16);
        put_u16(p+20,g_rolling_bemf.a);put_u16(p+22,g_rolling_bemf.b);put_u16(p+24,g_rolling_bemf.c);
        put_u16(p+26,age_ms);n=28u;break;
    }
    case 0xFDu: { /* RC12 live/latched observer handover state */
        const uint8_t live=g_motor_pipeline.powered;
        p[0]='R';p[1]='C';p[2]='1';p[3]='2';
        p[4]=live?ctrl.state:g_motor_pipeline.stop_foc_state;
        p[5]=live?ctrl.fault:g_motor_pipeline.stop_foc_fault;
        p[6]=g_power_armed;
        p[7]=g_motor_pipeline.last_stop_reason;
        put_u16(p+8, live?ctrl.observer_phase:g_motor_pipeline.stop_observer_phase);
        put_u16(p+10,live?ctrl.control_phase:g_motor_pipeline.stop_control_phase);
        put_u16(p+12,(uint16_t)(live?ctrl.phase_error:g_motor_pipeline.stop_phase_error));
        put_u16(p+14,(uint16_t)(live?ctrl.observer_step:g_motor_pipeline.stop_observer_step));
        put_u16(p+16,(uint16_t)(live?ctrl.id:g_motor_pipeline.stop_id));
        put_u16(p+18,(uint16_t)(live?ctrl.iq:g_motor_pipeline.stop_iq));
        put_u16(p+20,live?ctrl.rolling_good_ticks:g_motor_pipeline.stop_rolling_good_ticks);
        put_u16(p+22,live?ctrl.rolling_iq_ticks:g_motor_pipeline.stop_rolling_iq_ticks);
        put_u16(p+24,live?(uint16_t)(ctrl.vesc_flux.clamp_events-ctrl.rolling_clamp_start):
                              g_motor_pipeline.stop_clamp_delta);
        put_u16(p+26,(uint16_t)g_last_abs_current);
        n=28u;break;
    }
    case 0xFEu: { /* RC12 current/PWM/ADC fault context */
        const stock_phase_currents_t z=g_motor_pipeline.last_phase;
        uint16_t peak=abs16s(z.ia),q=abs16s(z.ib);
        if(q>peak)peak=q;q=abs16s(z.ic);if(q>peak)peak=q;
        p[0]='P';p[1]='I';p[2]='1';p[3]='2';
        put_u16(p+4,(uint16_t)z.ia);put_u16(p+6,(uint16_t)z.ib);put_u16(p+8,(uint16_t)z.ic);
        put_u16(p+10,peak);
        put_u16(p+12,ctrl.ccr1);put_u16(p+14,ctrl.ccr2);put_u16(p+16,ctrl.ccr3);
        put_u16(p+18,(uint16_t)ctrl.run_iq_counts);
        p[20]=g_motor_pipeline.last_sector;
        p[21]=g_motor_pipeline.adc.expected_sector;
        put_u16(p+22,(uint16_t)g_adc_raw[3]);
        put_u16(p+24,(uint16_t)g_safety_latched);
        p[26]=g_motor_pipeline.fault_latched;
        p[27]=g_gate_kill_latched;
        n=28u;break;
    }
    case 0xDAu: case 0xDBu: case 0xDCu: case 0xDDu: case 0xDEu: case 0xDFu:
        n=ble_motor_probe_read(cmd,p);break;
    default: n=0u;break;
    }
    if(requested!=0u&&requested<n)n=requested;ninebot_send_app_response(req,NINEBOT_READ_ACK,cmd,p,n);
}

static uint8_t has_magic(const ninebot_frame_t *f){return f->payload_len>=2u&&get_u16(f->payload)==MAGIC_UNLOCK;}
static void action_ack(const ninebot_frame_t *req,uint8_t cmd,uint8_t st) {
    g_last_action_status=st;
    if(req->cmd==NINEBOT_WRITE_NR)return;
    uint8_t p[1]={st};
    ninebot_send_app_response(req,NINEBOT_WRITE_ACK,cmd,p,1u);
}

static void start_offset_cal(void){
    irq_disable();
    g30_fixed_scan_start_offsets(&g_fixed_scan);
    g_offset_cal_valid=0u;
    g_offset_cal_active=1u;
    irq_enable();
}

static void app_write_power(const ninebot_frame_t *f) {
    uint16_t value=0u;
    if(f->payload_len>=2u)value=get_u16(f->payload);
    else if(f->payload_len>=1u)value=f->payload[0];
    action_ack(f,f->arg,ACT_OK);
    if(value!=0u)schedule_poweroff(POWER_OFF_ACK_DELAY_MS);
}

#if POWER_STAGE_ARM_ALLOWED
static uint8_t motor_request_rolling(void) {
#if POWER_STAGE_ARM_ALLOWED
    uint32_t primask_before;
    __asm volatile("mrs %0, primask" : "=r"(primask_before) :: "memory");
    irq_disable();
    uint8_t armed=0u;
    /* One atomic decision for both explicit E5 and dashboard operation.
     * IAP pending is checked here before any gate or ADC ownership change. */
    if(!g_power_armed && !g_motor_pipeline.powered &&
       !g_gate_kill_latched && !g_motor_id_vector_owner &&
       !g_offset_cal_active && !g_dash_brake &&
       g_dash_throttle>COMM_INPUT_IDLE_MAX && g_motor_pipeline.neutral_seen &&
       !drive_start_blockers(commissioning_guard_mask(),ROLLING_SENSORLESS_RC_ALLOWED) &&
       g30_rolling_bemf_ready(&g_rolling_bemf,DWT_CYCCNT)) {
        g_motor_irq_sequence=0u;
        g30_passive_bemf_adc_cancel(&g_passive_bemf_adc);
        armed=motor_pipeline_start_rolling(&g_motor_pipeline,
            (const uint16_t *)g_adc_offset,COMM_GATE_STOCK_PROFILE_READY &&
            (COMM_CURRENT_SCALE_HW_VALID || ROLLING_SENSORLESS_RC_ALLOWED),
            (COMM_ADC_TIMING_HW_VALID || ROLLING_SENSORLESS_RC_ALLOWED),
            g_dash_throttle,g_dash_brake,
            (uint8_t)(g_dash_seen && (uint32_t)(g_ms-g_dash_last_ms)<=COMM_DASH_STALE_MS),
            g_ms,g_rolling_bemf.phase_u16,g_rolling_bemf.electrical_step_u16);
    }
    if(!(primask_before&1u))irq_enable();
    return armed;
#else
    return 0u;
#endif
}
#endif

static void drive_runtime_poll(void) {
#if DRIVE_RUNTIME_ALLOWED
    if(!g_power_armed && !g_gate_kill_latched &&
       g_dash_throttle>COMM_INPUT_IDLE_MAX && !g_dash_brake)
        (void)motor_request_rolling();
#endif
}

static void app_write_action(const ninebot_frame_t *f){
    uint8_t cmd=f->arg;
    if(cmd==0xE6u){if(g_id_bench_requested)motor_id_auto_abort(&g_motor_id_auto);
        g_id_bench_requested=0u;
        power_stage_force_disarm();g_offset_cal_active=0u;g30_fixed_scan_abort(&g_fixed_scan);g_offset_cal_valid=0u;action_ack(f,cmd,ACT_OK);return;}
    if(!has_magic(f)){action_ack(f,cmd,ACT_BAD_MAGIC);return;}
    if(cmd==0xE0u){if(g_power_armed||g_safety_latched||g_poweroff_requested||
            iap_update_pending()||iap_update_active()||g_dash_throttle||g_dash_brake){
            action_ack(f,cmd,ACT_UNSAFE);return;}if(g_offset_cal_active){action_ack(f,cmd,ACT_BUSY);return;}start_offset_cal();action_ack(f,cmd,ACT_OK);return;}
    if(cmd==0xE1u){
        uint16_t guard=commissioning_guard_mask();
        action_ack(f,cmd,guard?ACT_UNSAFE:ACT_OK);return;
    }
    if(cmd>=0xE2u&&cmd<=0xE4u){action_ack(f,cmd,ACT_UNSUPPORTED);return;}
    if(cmd==0xE7u){
#if MOTOR_ID_BENCH_ALLOWED
        /* Existing NinebotCrypto BLE transport; authenticated app must send
         * magic C0DE + independent bench intent token 4D49. No UART start. */
        if(f->payload_len!=4u || get_u16(f->payload+2)!=ID_BENCH_TOKEN){
            action_ack(f,cmd,ACT_BAD_MAGIC);return;
        }
        if(g_gate_kill_latched || g_id_bench_requested || g_motor_id_vector_owner ||
           g_motor_id_auto.state!=ID_AUTO_IDLE || g_power_armed ||
           g_motor_pipeline.powered || ctrl.drive_request ||
           g_safety_latched || g_poweroff_requested ||
           iap_update_active() || iap_update_pending() ||
           !g_offset_cal_valid || !g_adc2_ch4_offset_valid ||
           !g_adc2_ch5_offset_valid || !g_dash_seen ||
           g_dash_brake || g_dash_throttle>COMM_INPUT_IDLE_MAX ||
           (uint32_t)(g_ms-g_dash_last_ms)>COMM_DASH_STALE_MS ||
           (uint32_t)(g_ms-g_adc_last_ms)>ADC_STALE_MS ||
           (uint32_t)(g_ms-g_vbus_last_ms)>100u ||
           g_adc_raw[3]<500u || g_adc_raw[3]>3200u ||
           g_last_abs_current>ARM_IDLE_CURRENT_COUNTS){
            action_ack(f,cmd,ACT_UNSAFE);return;
        }
        /* Hardware-derived prerequisites, no diagnostic fake samples.
         * Current engineering conversion is stock nominal, monitored
         * concurrently by a strict unscaled ADC trip. */
        const motor_id_qualification_t q={
            .gate_shutdown_verified=(uint8_t)(!g_power_armed &&
                stock_tim1_gate_ccer_is_off((uint16_t)TIM_CCER(TIM1_BASE))),
            .current_scale_verified=COMM_CURRENT_SCALE_HW_VALID,
            .sample_timing_verified=COMM_ADC_TIMING_HW_VALID,
            .bench_provisional_scale=1u, /* nominal DRV126 gain, NOT verified */
            .bench_adc_runtime_guard=1u, /* raw offset trip + pair synchrony */
            .bus_sense_verified=(uint8_t)(g_vbus_pending==0u),
            .overcurrent_protection_verified=1u, /* independent ADC counts trip */
            .wheel_unloaded=1u /* operator confirms by E7 BENCH token */
        };
        uint32_t prior_primask;
        __asm volatile("mrs %0, primask" : "=r"(prior_primask) :: "memory");
        irq_disable();
        g_id_bench_requested=1u;
        motor_id_flux_feedback_reset(&g_motor_id_flux_feedback);
        g_id_bench_started_ms=g_ms;
        g_id_bench_energized_since_ms=0u;
        /* Repeat detect after one completed/faulted session only after reset;
         * avoid silent rearming after an anomaly. */
        const uint8_t started=motor_id_auto_begin(&g_motor_id_auto,&q,
                                       DWT_CYCCNT/(SYSCLK_HZ/1000000u));
        if(!started){g_id_bench_requested=0u;power_stage_force_disarm();}
        if(!(prior_primask&1u))irq_enable();
        action_ack(f,cmd,started?ACT_OK:ACT_UNSAFE);
#else
        action_ack(f,cmd,ACT_UNSUPPORTED);
#endif
        return;
    }
    if(cmd==0xE5u){
        if(g_motor_id_vector_owner || g_power_armed || ctrl.drive_request){action_ack(f,cmd,ACT_BUSY);return;}
        if(f->payload_len<4u){action_ack(f,cmd,ACT_BAD_RANGE);return;}uint16_t ma=get_u16(f->payload+2);
        if(!sensorless_control_set_run_current_ma(&ctrl,ma)){action_ack(f,cmd,ACT_BAD_RANGE);return;}
        g_cfg_test_current_ma=ma;
#if POWER_STAGE_ARM_ALLOWED
#if DRIVE_RUNTIME_ALLOWED
        ctrl.run_iq_counts=drive_throttle_counts(g_dash_throttle,g_dash_brake,
            COMM_INPUT_IDLE_MAX,stock_current_nominal_ma_to_counts(ctrl.run_current_ma));
#endif
        const uint8_t armed=motor_request_rolling();
        action_ack(f,cmd,armed?ACT_OK:ACT_UNSAFE);return;
#else
        action_ack(f,cmd,ACT_UNSUPPORTED);return;
#endif
    }
    action_ack(f,cmd,ACT_UNSUPPORTED);
}

/* Two-phase RAM transaction. The previous F0/F1/F2 direct writes were
 * unsafe for the observer: partial tuples can alter the motor model. */
static void sync_active_config_from_txn(void) {
    g_cfg_r_uohm=g_motor_cfg.active.r_uohm;
    g_cfg_l_nh=g_motor_cfg.active.l_nh;
    g_cfg_flux_uwb=g_motor_cfg.active.flux_uwb;
    g_cfg_phase_offset=g_motor_cfg.active.phase_offset;
    g_cfg_test_current_ma=g_motor_cfg.active.test_current_ma;
}
static uint8_t commit_motor_config(void) {
    if(!motor_config_pending_complete(&g_motor_cfg))return 0u;
    const motor_config_values_t v=g_motor_cfg.pending;
    irq_disable();
    uint8_t ok=(uint8_t)(sensorless_control_set_motor_params(&ctrl,v.r_uohm,v.l_nh,v.flux_uwb) &&
                         sensorless_control_set_run_current_ma(&ctrl,v.test_current_ma));
    if(ok) {
        sensorless_control_set_phase_offset(&ctrl,v.phase_offset);
        /* Passive seed and powered observer use the same committed frame. */
        g_rolling_bemf.cfg.phase_offset_u16=v.phase_offset;
        g30_rolling_bemf_reset(&g_rolling_bemf);
        ok=motor_config_commit(&g_motor_cfg);
        if(ok){sync_active_config_from_txn();g_cfg_dirty=1u;}
    }
    irq_enable();
    return ok;
}

static void motor_profile_from_build(void) {
#if DRIVE_RUNTIME_ALLOWED
    /* Startup only, before peripheral IRQs. Use the existing range-checked
     * transaction before touching ctrl, so partial/invalid tuples do not
     * become a motor profile. No persistent flash or recovery state here. */
    if(!G30_PROFILE_POLE_PAIRS || G30_PROFILE_POLE_PAIRS>64u ||
       G30_PROFILE_CIRCUMFERENCE_MM<300u || G30_PROFILE_CIRCUMFERENCE_MM>1800u)
        return;
    motor_config_txn_t profile;
    motor_config_txn_init(&profile,G30_PROFILE_CURRENT_MA);
    if(!motor_config_stage_u32(&profile,0xf0u,G30_PROFILE_R_UOHM) ||
       !motor_config_stage_u32(&profile,0xf1u,G30_PROFILE_L_NH) ||
       !motor_config_stage_u32(&profile,0xf2u,G30_PROFILE_FLUX_UWB) ||
       !motor_config_stage_i16(&profile,(int16_t)G30_PROFILE_PHASE_OFFSET_U16) ||
       !motor_config_stage_u16(&profile,G30_PROFILE_CURRENT_MA) ||
       !motor_config_commit(&profile))return;
    const motor_config_values_t *v=&profile.active;
    if(!sensorless_control_set_motor_params(&ctrl,v->r_uohm,v->l_nh,v->flux_uwb) ||
       !sensorless_control_set_run_current_ma(&ctrl,v->test_current_ma))return;
    sensorless_control_set_phase_offset(&ctrl,v->phase_offset);
    g_motor_cfg=profile;
    sync_active_config_from_txn();
#endif
}
static void app_write_config(const ninebot_frame_t *f) {
    const uint8_t cmd=f->arg;
    /* NEVER stage while drive, ADC calibration, IAP or poweroff is active. */
    if(g_power_armed || ctrl.drive_request || ctrl.state!=SENSORLESS_STOP ||
       g_offset_cal_active || g_poweroff_requested || iap_update_active() ||
       iap_update_pending()) {action_ack(f,cmd,ACT_UNSAFE);return;}
#if DRIVE_RUNTIME_ALLOWED
    if(g_dash_throttle>COMM_INPUT_IDLE_MAX || g_dash_brake) {
        action_ack(f,cmd,ACT_UNSAFE);return;
    }
#endif
    if(!has_magic(f)){action_ack(f,cmd,ACT_BAD_MAGIC);return;}
    if(cmd==0xFFu) {
        /* Measured wheel geometry, RAM only; never writes calibration over
         * staging, control metadata, the application or the stock loader. */
        if(f->payload_len!=5u || !f->payload[2] || f->payload[2]>64u ||
           get_u16(f->payload+3)<300u || get_u16(f->payload+3)>1800u) {
            action_ack(f,cmd,ACT_BAD_RANGE);return;
        }
        irq_disable();
        g_rolling_bemf.cfg.pole_pairs=f->payload[2];
        g_rolling_bemf.cfg.wheel_circ_mm=get_u16(f->payload+3);
        g30_rolling_bemf_reset(&g_rolling_bemf);
        irq_enable();
        action_ack(f,cmd,ACT_RAM_ONLY);return;
    }
    if(cmd>=0xF0u&&cmd<=0xF2u) {
        if(f->payload_len!=6u || !motor_config_stage_u32(&g_motor_cfg,cmd,get_u32(f->payload+2))) {
            action_ack(f,cmd,ACT_BAD_RANGE);return;
        }
        action_ack(f,cmd,ACT_RAM_ONLY);return;
    }
    if(cmd==0xF3u) {
        if(f->payload_len!=4u){action_ack(f,cmd,ACT_BAD_RANGE);return;}
        (void)motor_config_stage_i16(&g_motor_cfg,get_i16(f->payload+2));
        action_ack(f,cmd,ACT_RAM_ONLY);return;
    }
    if(cmd==0xF4u) {
        if(f->payload_len!=4u || !motor_config_stage_u16(&g_motor_cfg,get_u16(f->payload+2))) {
            action_ack(f,cmd,ACT_BAD_RANGE);return;
        }
        action_ack(f,cmd,ACT_RAM_ONLY);return;
    }
    if(cmd==0xF5u) {
        if(f->payload_len==3u && f->payload[2]==0u) {
            motor_config_abort(&g_motor_cfg);action_ack(f,cmd,ACT_OK);return;
        }
        if(f->payload_len!=2u || !commit_motor_config()) {
            action_ack(f,cmd,ACT_BAD_RANGE);return;
        }
        action_ack(f,cmd,ACT_RAM_ONLY);return;
    }
    action_ack(f,cmd,ACT_UNSUPPORTED);
}

static uint8_t handle_stock_dashboard(const ninebot_frame_t *f){
    uint8_t throttle=0u,brake=0u;
    if(dashboard_decode_adc(f,&throttle,&brake)){
        g_dash_throttle=throttle;g_dash_brake=brake;
#if DRIVE_RUNTIME_ALLOWED
        ctrl.run_iq_counts=drive_throttle_counts(throttle,brake,COMM_INPUT_IDLE_MAX,
                         stock_current_nominal_ma_to_counts(ctrl.run_current_ma));
#endif
        motor_pipeline_input(&g_motor_pipeline,throttle,brake,1u,g_ms);
        return 1u;
    }
    if(dashboard_is_status_request(f)){
        uint8_t p[G30_DASH_STATUS_BYTES];
#if DRIVE_RUNTIME_ALLOWED
        const uint16_t speed=g_power_armed && ctrl.observer_valid ?
            drive_speed_kmh10(ctrl.observer_step,g_rolling_bemf.cfg.pole_pairs,
                             g_rolling_bemf.cfg.wheel_circ_mm) :
            g30_rolling_bemf_speed_kmh10(&g_rolling_bemf,DWT_CYCCNT);
        /* Battery remains the explicit historical placeholder until BMS
         * telemetry is integrated. Do not invent pack SOC from raw VBUS. */
        dashboard_drive_status_payload(p,speed,50u,(uint8_t)(current_fault_code()!=0u));
#else
        dashboard_safe_status_payload(p);
#endif
        ninebot_send_app_response(f,G30_DASH_CMD_STATUS,0x00u,p,G30_DASH_STATUS_BYTES);
        return 1u;
    }
    return 0u;
}

static void process_frame(const ninebot_frame_t *f){
    if(f->format==NINEBOT_FMT_5AA5&&f->src==G30_DASH_ADDR&&f->dst==G30_ESC_ADDR){
        g_dash_seen=1u;g_dash_frames++;g_dash_last_ms=g_ms;
        if(handle_stock_dashboard(f))return;
    }
    if(f->dst==0x20u&&(f->cmd==NINEBOT_READ||f->cmd==NINEBOT_WRITE||f->cmd==NINEBOT_WRITE_NR)){
        if(f->cmd==NINEBOT_READ)app_read_response(f);
        else if(f->arg==0x79u)app_write_power(f);
        else if((f->arg>=0xE0u&&f->arg<=0xE6u)||f->arg==0xE7u)app_write_action(f);
        else if((f->arg>=0xF0u&&f->arg<=0xF5u)||f->arg==0xFFu)app_write_config(f);
        else action_ack(f,f->arg,ACT_UNSUPPORTED);
        return;
    }
}

static void reset_stats(void){g_sample_interval_min=0xFFFFFFFFu;g_sample_interval_max=0u;g_max_isr_cycles=0u;g_max_control_cycles=0u;g_peak_abs_current=0u;g_control_hard_overruns=0u;}
static void report(void){
    uart_puts("DeltaESC v0.10.3 arm=");uart_u32(g_power_armed);uart_puts(" state=");uart_puts(sensorless_state_name(ctrl.state));uart_puts(" obs=");uart_u32(ctrl.observer_valid);
    uart_puts(" ctrl=");uart_u32(g_last_control_cycles);uart_putc('/');uart_u32(g_max_control_cycles);uart_puts(" I=");uart_u32(g_last_abs_current);uart_putc('/');uart_u32(g_peak_abs_current);
    uart_puts(" dash=");uart_u32(g_dash_frames);uart_puts(" fmt=");uart_u32(ninebot_link_last_format());uart_puts(" thr/brk=");uart_u32(g_dash_throttle);uart_putc('/');uart_u32(g_dash_brake);uart_puts(" rxov=");uart_u32(ninebot_link_rx_overflows());
    uart_puts(" action=");uart_u32(g_last_action_status);uart_puts(" fault=");uart_u32(current_fault_code());uart_puts("\r\n");
}

static void handle_debug_command(int ch){
    if(ch=='d'||ch=='D'){power_stage_force_disarm();uart_puts("DISARMED\r\n");}
    else if(ch=='c'||ch=='C'){reset_stats();uart_puts("stats cleared\r\n");}
    else if(ch=='?'||ch=='h'||ch=='H'){uart_puts("commands: ? status, D disarm, C clear stats; drive target uses stock dashboard input\r\n");report();}
}

int main(void){
    irq_disable();power_hold_init();clock_64mhz_hsi();dwt_init();gate_pins_to_safe_inputs();uart1_debug_init();sensorless_control_init(&ctrl);
#if ROLLING_SENSORLESS_RC_ALLOWED && !DRIVE_RUNTIME_ALLOWED
    if(!sensorless_control_set_motor_params(&ctrl,FO70_R_UOHM,FO70_L_NH,FO70_FLUX_UWB) ||
       !sensorless_control_set_run_current_ma(&ctrl,FO70_RUN_CURRENT_MA)) {
        g_safety_latched=0xA020u;
    }
    sensorless_control_set_phase_offset(&ctrl,(int16_t)FO70_FRAME_OFFSET_U16);
#endif
    const motor_pipeline_io_t motor_io={motor_hw_arm,motor_hw_gate_off,
                                         motor_hw_select_adc,motor_hw_pwm,motor_hw_coast};
    motor_pipeline_init(&g_motor_pipeline,&ctrl,motor_io,0,PWM_ARR,HARD_OC_COUNTS);
    motor_config_txn_init(&g_motor_cfg,g_cfg_test_current_ma);
    motor_profile_from_build();
    /* The real TIM1 vector callback is connected, but cannot arm in a
     * sync-safe build. Current and voltage feedback remain a separate task. */
    motor_id_adc_feedback_init(&g_motor_id_feedback);
    NVIC_IPR8(IRQ_TIM1_UP)=0x10u;
    NVIC_ISER0=(1u<<IRQ_TIM1_UP);
    const motor_id_vector_io_t vector_io={motor_hw_select_adc,motor_hw_pwm,
                                         motor_hw_arm,motor_id_hw_gate_off};
    motor_id_vector_output_init(&g_motor_id_vector,vector_io,0);
    const motor_id_auto_io_t id_io={motor_id_hw_vector_mv,motor_id_hw_gate_off,0};
    motor_id_auto_init(&g_motor_id_auto,id_io,0,&ctrl);
    motor_id_auto_bind_config(&g_motor_id_auto,&g_motor_cfg);
    motor_probe_init();power_button_init(&g_power_button_state,POWER_BUTTON_LONG_MS);
    adc1_injected_init();tim1_pwm_and_adc_trigger_init();passive_bemf_init();ninebot_link_init();iap_update_init();systick_init();irq_enable();
    uart_puts("DeltaESC G30D v0.10.3 source candidate.\r\n");
#if DRIVE_RUNTIME_ALLOWED
    uart_puts("BUILD: low-current rolling drive; measured F0-F5/FF RAM profile required.\r\n");
#elif MOTOR_ID_BENCH_ALLOWED
    uart_puts("BUILD: live Motor-ID BLE E7 only; normal drive disabled.\r\n");
#elif SENSORLESS_RUN_ALLOWED
    uart_puts("BUILD: SENSORLESS BENCH; BLE E5 may arm/start only after safety preconditions.\r\n");
#elif POWER_STAGE_ARM_ALLOWED
    uart_puts("BUILD: OBSERVER ZERO-VECTOR; BLE E5 arms zero-vector only, motor run compiled out.\r\n");
#else
    uart_puts("BUILD: BLE SYNC-SAFE; bridge arming and motor run compiled out.\r\n");
#endif
    uart_puts("PA2 USART2 115200 half-duplex; native G30 0x64/0x65 runtime + D0-D9/E0-E6/F0-F5; staged IAP enabled.\r\n");
    uart_puts("BLE E0 offsets, E6 terminal STOP, F7/F8/F9 status; IAP commands unchanged.\r\n");
    uint32_t next=1000u;
    for(;;){
        int ch=uart_getc_nonblock();if(ch>=0)handle_debug_command(ch);
        uint8_t iap_owned=(uint8_t)(iap_update_pending()||iap_update_active());
        if(iap_owned){if(g_id_bench_requested)motor_id_auto_abort(&g_motor_id_auto);
                      g_id_bench_requested=0u;
                      power_stage_force_disarm();g_offset_cal_active=0u;g30_fixed_scan_abort(&g_fixed_scan);g_offset_cal_valid=0u;}
        /* Do not wait for the 1s report to recognize a stalled ADC IRQ. */
        if(g_adc_samples && (uint32_t)(g_ms-g_adc_last_ms)>ADC_STALE_MS && !g_safety_latched){
            g_safety_latched=0xA005u;power_stage_force_disarm();
        }
        adc_vbus_poll();
        passive_bemf_poll();
        iap_update_poll();
        if(iap_owned||iap_update_active()) ninebot_link_discard_rx();
        else { ninebot_frame_t f;while(ninebot_link_get_frame(&f))process_frame(&f); }
        drive_runtime_poll();
        if((int32_t)(g_ms-next)>=0){next+=1000u;report();if((uint32_t)(g_ms-g_adc_last_ms)>ADC_STALE_MS){g_safety_latched=0xA002u;power_stage_force_disarm();}if(g_max_control_cycles>CONTROL_WARN_CYCLES)uart_puts("WARN: 4kHz control reserve below 25%\r\n");}
        if(g_id_bench_requested &&
           (g_motor_id_auto.state==ID_AUTO_COMPLETE || g_motor_id_auto.state==ID_AUTO_FAULT)){
            g_id_bench_requested=0u;
            power_stage_force_disarm();
        }
        if(!gate_safety_check())uart_puts("FAULT: PWM safety envelope violated; DISARMED\r\n");
        if(g_poweroff_requested) {
            /* Cancel an earlier button/0x79 request if any update starts. */
            if(iap_update_active())g_poweroff_requested=0u;
            else if((int32_t)(g_ms-g_poweroff_deadline_ms)>=0)power_hold_release_now();
        }
        __asm volatile("wfi");
    }
}
