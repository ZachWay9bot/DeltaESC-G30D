#include "ble_motor_probe.h"
#include "motor_probe.h"

static void le16(uint8_t *p,uint16_t v){p[0]=(uint8_t)v;p[1]=(uint8_t)(v>>8);}
static void le32(uint8_t *p,uint32_t v){
    le16(p,(uint16_t)v);le16(p+2,(uint16_t)(v>>16));
}

/* The ADC ISR updates this struct once per 256 samples, bracketing the
   update with odd/even sequence numbers. Bound all snapshot attempts so
   a bad ADC state can never hang BLE/dashboard responsiveness. */
static uint8_t snapshot(motor_probe_t *p) {
    for(unsigned tries=0u;tries<3u;tries++){
        uint32_t first=g_motor_probe.sequence;
        if(first&1u)continue;
        p->magic=g_motor_probe.magic;
        p->sequence=first;
        p->windows=g_motor_probe.windows;
        p->total_samples=g_motor_probe.total_samples;
        p->stamp_ms=g_motor_probe.stamp_ms;
        p->gate_armed=g_motor_probe.gate_armed;
        p->adc_jsqr=g_motor_probe.adc_jsqr;
        p->adc_cr2=g_motor_probe.adc_cr2;
        p->tim1_ccer=g_motor_probe.tim1_ccer;
        p->tim1_bdtr=g_motor_probe.tim1_bdtr;
        p->sample_period_min_cycles=g_motor_probe.sample_period_min_cycles;
        p->sample_period_max_cycles=g_motor_probe.sample_period_max_cycles;
        p->max_control_cycles=g_motor_probe.max_control_cycles;
        for(unsigned i=0u;i<4u;i++){
            p->mean_adc[i]=g_motor_probe.mean_adc[i];
            p->min_adc[i]=g_motor_probe.min_adc[i];
            p->max_adc[i]=g_motor_probe.max_adc[i];
            p->last_adc[i]=g_motor_probe.last_adc[i];
        }
        for(unsigned i=0u;i<3u;i++)p->offset_adc[i]=g_motor_probe.offset_adc[i];
        p->window_size=g_motor_probe.window_size;
        __asm volatile ("" ::: "memory");
        uint32_t last=g_motor_probe.sequence;
        if(first==last && (last&1u)==0u && p->magic==MOTOR_PROBE_MAGIC)return 1u;
    }
    return 0u;
}

uint8_t ble_motor_probe_read(uint8_t reg,uint8_t out[16]) {
    if(!out||reg<0xDAu||reg>0xDFu)return 0u;
    motor_probe_t p;
    if(!snapshot(&p))return 0u;
    for(unsigned i=0u;i<16u;i++)out[i]=0u;
    switch(reg){
    case 0xDAu:
        le32(out,p.magic);le32(out+4,p.sequence);
        le32(out+8,p.windows);le32(out+12,p.stamp_ms);break;
    case 0xDBu:
        for(unsigned i=0u;i<4u;i++)le16(out+i*2u,p.mean_adc[i]);
        for(unsigned i=0u;i<4u;i++)le16(out+8u+i*2u,p.min_adc[i]);
        break;
    case 0xDCu:
        for(unsigned i=0u;i<4u;i++)le16(out+i*2u,p.max_adc[i]);
        for(unsigned i=0u;i<4u;i++)le16(out+8u+i*2u,p.last_adc[i]);
        break;
    case 0xDDu:
        for(unsigned i=0u;i<3u;i++)le16(out+i*2u,p.offset_adc[i]);
        le16(out+6,p.window_size);
        le32(out+8,p.gate_armed);le32(out+12,p.total_samples);break;
    case 0xDEu:
        le32(out,p.sample_period_min_cycles);
        le32(out+4,p.sample_period_max_cycles);
        le32(out+8,p.max_control_cycles);le32(out+12,p.tim1_ccer);break;
    case 0xDFu:
        le32(out,p.adc_jsqr);le32(out+4,p.adc_cr2);
        le32(out+8,p.tim1_bdtr);
        le32(out+12,((uint32_t)BLE_PROBE_VERSION<<16)|(p.windows?1u:0u));break;
    default:return 0u;
    }
    return 16u;
}
