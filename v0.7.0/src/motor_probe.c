#include "motor_probe.h"
volatile motor_probe_t g_motor_probe;
static uint32_t sum[4];
static uint16_t low[4],high[4];
static uint16_t count;
void motor_probe_init(void) {
    g_motor_probe.magic=MOTOR_PROBE_MAGIC;
    g_motor_probe.sequence=0u;
    g_motor_probe.windows=0u;
    g_motor_probe.total_samples=0u;
    g_motor_probe.window_size=MOTOR_PROBE_WINDOW;
    count=0u;
    for(unsigned i=0u;i<4u;i++){
        sum[i]=0u;low[i]=4095u;high[i]=0u;
        g_motor_probe.mean_adc[i]=0u;g_motor_probe.min_adc[i]=0u;
        g_motor_probe.max_adc[i]=0u;g_motor_probe.last_adc[i]=0u;
    }
}
void motor_probe_capture(uint16_t s0,uint16_t s1,uint16_t s2,uint16_t s3,
                         const volatile uint16_t offset[3],uint32_t ms,
                         uint32_t sample_period_min,uint32_t sample_period_max,
                         uint32_t control_max_cycles,uint32_t gate_armed,
                         uint32_t adc_jsqr,uint32_t adc_cr2,
                         uint32_t tim1_ccer,uint32_t tim1_bdtr) {
    const uint16_t x[4]={s0,s1,s2,s3};
    for(unsigned i=0u;i<4u;i++){
        sum[i]+=x[i];
        if(x[i]<low[i])low[i]=x[i];
        if(x[i]>high[i])high[i]=x[i];
    }
    count++;
    if(count<MOTOR_PROBE_WINDOW)return;
    g_motor_probe.sequence++;
    for(unsigned i=0u;i<4u;i++){
        g_motor_probe.mean_adc[i]=(uint16_t)(sum[i]/MOTOR_PROBE_WINDOW);
        g_motor_probe.min_adc[i]=low[i];g_motor_probe.max_adc[i]=high[i];
        g_motor_probe.last_adc[i]=x[i];
        sum[i]=0u;low[i]=4095u;high[i]=0u;
    }
    for(unsigned i=0u;i<3u;i++)g_motor_probe.offset_adc[i]=offset[i];
    g_motor_probe.windows++;g_motor_probe.total_samples+=MOTOR_PROBE_WINDOW;
    g_motor_probe.stamp_ms=ms;g_motor_probe.gate_armed=gate_armed;
    g_motor_probe.sample_period_min_cycles=sample_period_min;
    g_motor_probe.sample_period_max_cycles=sample_period_max;
    g_motor_probe.max_control_cycles=control_max_cycles;
    g_motor_probe.adc_jsqr=adc_jsqr;g_motor_probe.adc_cr2=adc_cr2;
    g_motor_probe.tim1_ccer=tim1_ccer;g_motor_probe.tim1_bdtr=tim1_bdtr;
    count=0u;g_motor_probe.sequence++;
}
