#pragma once
#include <stdint.h>
#define MOTOR_PROBE_MAGIC 0x3052544Du /* ASCII MTR0 in little endian */
#define MOTOR_PROBE_WINDOW 256u
typedef struct {
    uint32_t magic;
    uint32_t sequence;
    uint32_t windows;
    uint32_t total_samples;
    uint32_t stamp_ms;
    uint32_t gate_armed;
    uint32_t adc_jsqr;
    uint32_t adc_cr2;
    uint32_t tim1_ccer;
    uint32_t tim1_bdtr;
    uint32_t sample_period_min_cycles;
    uint32_t sample_period_max_cycles;
    uint32_t max_control_cycles;
    uint16_t mean_adc[4];
    uint16_t min_adc[4];
    uint16_t max_adc[4];
    uint16_t last_adc[4];
    uint16_t offset_adc[3];
    uint16_t window_size;
} motor_probe_t;
extern volatile motor_probe_t g_motor_probe;
void motor_probe_init(void);
void motor_probe_capture(uint16_t s0,uint16_t s1,uint16_t s2,uint16_t s3,
                         const volatile uint16_t offset[3],uint32_t ms,
                         uint32_t sample_period_min,uint32_t sample_period_max,
                         uint32_t control_max_cycles,uint32_t gate_armed,
                         uint32_t adc_jsqr,uint32_t adc_cr2,
                         uint32_t tim1_ccer,uint32_t tim1_bdtr);
