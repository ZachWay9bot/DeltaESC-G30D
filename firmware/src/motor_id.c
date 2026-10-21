#include "motor_id.h"
#include <stddef.h>

#define ID_MIN_SAMPLES 8u
#define ID_MAX_STAGE_US 3000000u
#define ID_MAX_DT_US 100000u
/* Absolute bounds mirror the existing sensorless parameter ABI. */
#define ID_R_MIN 1000u
#define ID_R_MAX 2000000u
#define ID_L_MIN 1000u
#define ID_L_MAX 5000000u
#define ID_FLUX_MIN 100u
#define ID_FLUX_MAX 1000000u

/* No libgcc/compiler-rt assumed on the bare-metal F103. Clang otherwise
 * emits __aeabi_uldivmod for uint64_t/uint32_t and a host-only suite
 * can falsely claim the embedded firmware links. We bound overflow and use
 * a 32-step long division with 64-bit remainder to keep it generic. */
static uint32_t divide_u64_u32(uint64_t numerator, uint32_t denominator) {
    if (!denominator || (numerator >> 32) >= denominator) return 0xffffffffu;
    uint32_t quotient=0u;
    uint32_t remainder=(uint32_t)(numerator>>32);
    for(int bit=31;bit>=0;--bit) {
        uint64_t shifted=((uint64_t)remainder<<1)|(((uint32_t)numerator>>(unsigned)bit)&1u);
        if(shifted >= denominator) {
            shifted-=denominator;
            quotient|=1u<<(unsigned)bit;
        }
        remainder=(uint32_t)shifted;
    }
    return quotient;
}

static uint8_t fail(motor_id_t *m, uint8_t why) {
    m->fault=(uint8_t)(m->fault|why);
    m->state=MOTOR_ID_FAULT;
    m->result.valid=0u;
    m->result.r_uohm=0u;
    m->result.l_nh=0u;
    m->result.flux_uwb=0u;
    return 0u;
}
static uint32_t abs_i32(int32_t v) {return v<0?(uint32_t)(-(int64_t)v):(uint32_t)v;}
static void reset_accumulator(motor_id_t *m) {
    m->sum=0u; m->samples=0u; m->elapsed_stage_us=0u;
    m->min_value=0xffffffffu; m->max_value=0u;
}
void motor_id_init(motor_id_t *m) {
    if (!m) return;
    volatile uint8_t *p=(volatile uint8_t *)m;
    for(uint32_t i=0u;i<(uint32_t)sizeof(*m);i++)p[i]=0u;
    m->state=MOTOR_ID_IDLE;
    m->target_samples_r=ID_MIN_SAMPLES;
    m->target_samples_l=ID_MIN_SAMPLES;
    m->target_samples_flux=ID_MIN_SAMPLES;
}
uint8_t motor_id_set_sample_targets(motor_id_t *m, uint8_t r, uint8_t l, uint8_t flux) {
    if(!m || m->state!=MOTOR_ID_RESISTANCE || m->samples ||
       r<8u || l<8u || flux<8u || r>96u || l>96u || flux>96u) return 0u;
    m->target_samples_r=r;m->target_samples_l=l;m->target_samples_flux=flux;
    return 1u;
}
void motor_id_abort(motor_id_t *m) {
    if(!m) return;
    (void)fail(m,MOTOR_ID_ABORT);
}
uint8_t motor_id_begin(motor_id_t *m, const motor_id_qualification_t *q) {
    if (!m) return 0u;
    motor_id_init(m);
    if (!q || !q->gate_shutdown_verified ||
        !((q->current_scale_verified && q->sample_timing_verified) ||
          (q->bench_provisional_scale && q->bench_adc_runtime_guard &&
           q->overcurrent_protection_verified)) ||
        !q->bus_sense_verified || !q->overcurrent_protection_verified ||
        !q->wheel_unloaded)
        return fail(m,MOTOR_ID_BAD_QUALIFICATION);
    m->state=MOTOR_ID_RESISTANCE;reset_accumulator(m);
    return 1u;
}
static uint8_t complete_stage(motor_id_t *m) {
    const uint32_t target=m->state==MOTOR_ID_RESISTANCE?m->target_samples_r:
        m->state==MOTOR_ID_INDUCTANCE?m->target_samples_l:m->target_samples_flux;
    if(m->samples<target) return 1u;
    /* Reject samples whose variation is > +/-20% of their mean. */
    uint32_t avg=divide_u64_u32(m->sum,m->samples);
    if (!avg || (uint64_t)(m->max_value-m->min_value)*5u > avg)
        return fail(m,MOTOR_ID_INCONSISTENT);
    switch(m->state) {
    case MOTOR_ID_RESISTANCE:
        m->result.r_uohm=avg; m->result.samples_r=m->samples;
        m->state=MOTOR_ID_INDUCTANCE; break;
    case MOTOR_ID_INDUCTANCE:
        m->result.l_nh=avg; m->result.samples_l=m->samples;
        m->state=MOTOR_ID_FLUX;break;
    case MOTOR_ID_FLUX:
        m->result.flux_uwb=avg; m->result.samples_flux=m->samples;
        m->result.valid=1u;m->state=MOTOR_ID_READY;break;
    default: return fail(m,MOTOR_ID_BAD_SAMPLE);
    }
    reset_accumulator(m);
    return 1u;
}
uint8_t motor_id_accept(motor_id_t *m, const motor_id_sample_t *s) {
    if(!m || m->state==MOTOR_ID_FAULT || m->state==MOTOR_ID_IDLE || m->state==MOTOR_ID_READY)
        return 0u;
    if(!s || s->fault_active || s->brake_active || s->stop_requested || !s->samples_synchronized)
        return fail(m,MOTOR_ID_BAD_SAMPLE);
    if(m->seen_seq && s->sequence!=m->last_seq+1u)
        return fail(m,MOTOR_ID_SEQUENCE);
    if(s->stage!=m->state || !s->elapsed_us || s->elapsed_us>ID_MAX_DT_US)
        return fail(m,MOTOR_ID_BAD_SAMPLE);
    if(m->elapsed_stage_us>ID_MAX_STAGE_US-s->elapsed_us)
        return fail(m,MOTOR_ID_TIMEOUT);
    m->elapsed_stage_us+=s->elapsed_us;
    m->last_seq=s->sequence;m->seen_seq=1u;
    uint32_t value=0u;
    uint64_t numerator=0u;
    uint32_t denominator=0u;
    if(m->state==MOTOR_ID_RESISTANCE) {
        /* Locked motor, steady current: R = V/I. Reject zero/reversed current,
         * excessive test current and a still-turning rotor. */
        if(s->electrical_speed_mrad_s!=0u ||
           abs_i32(s->current_ma)<200u || abs_i32(s->current_ma)>2000u ||
           s->current_ma<0 || s->voltage_mv<=0 || s->voltage_mv>3000)
            return fail(m,MOTOR_ID_RANGE);
        numerator=(uint64_t)(uint32_t)s->voltage_mv*1000000u;
        denominator=(uint32_t)s->current_ma;
    } else if(m->state==MOTOR_ID_INDUCTANCE) {
        /* One current derivative interval, with previously determined R.
         * L = (V - I*R) * dt / di. Values use mV,mA,us,nH. */
        if(s->electrical_speed_mrad_s!=0u ||
           s->current_ma<0 || s->current_ma>2000 ||
           s->previous_current_ma<0 || s->previous_current_ma>2000 ||
           s->current_ma<=s->previous_current_ma ||
           s->voltage_mv<=0 || s->voltage_mv>3000)
            return fail(m,MOTOR_ID_RANGE);
        uint32_t di=(uint32_t)(s->current_ma-s->previous_current_ma);
        uint32_t resistive_mv=divide_u64_u32((uint64_t)(uint32_t)s->current_ma*m->result.r_uohm,1000000u);
        int64_t inductive_mv=(int64_t)s->voltage_mv-resistive_mv;
        if(inductive_mv<1 || di<1) return fail(m,MOTOR_ID_RANGE);
        numerator=(uint64_t)inductive_mv*s->elapsed_us*1000u;
        denominator=di;
    } else {
        /* E = omega_e * lambda, electrical speed in mrad/s.
         * Magnitudes only: direction/sign separately qualified. */
        if(s->electrical_speed_mrad_s<1000u || s->electrical_speed_mrad_s>200000u ||
           s->bemf_mv<10u || s->bemf_mv>60000u ||
           abs_i32(s->current_ma)>2000u)
            return fail(m,MOTOR_ID_RANGE);
        numerator=(uint64_t)s->bemf_mv*1000000u;
        denominator=s->electrical_speed_mrad_s;
    }
    if(!denominator) return fail(m,MOTOR_ID_RANGE);
    value=divide_u64_u32(numerator,denominator);
    if(value==0xffffffffu) return fail(m,MOTOR_ID_RANGE);
    uint32_t lo=m->state==MOTOR_ID_RESISTANCE?ID_R_MIN:(m->state==MOTOR_ID_INDUCTANCE?ID_L_MIN:ID_FLUX_MIN);
    uint32_t hi=m->state==MOTOR_ID_RESISTANCE?ID_R_MAX:(m->state==MOTOR_ID_INDUCTANCE?ID_L_MAX:ID_FLUX_MAX);
    if(value<lo || value>hi) return fail(m,MOTOR_ID_RANGE);
    m->sum+=value;
    if(value<m->min_value)m->min_value=value;
    if(value>m->max_value)m->max_value=value;
    m->samples++;
    return complete_stage(m);
}
uint8_t motor_id_commit(const motor_id_t *m, sensorless_control_t *c) {
    if(!m || !c || m->state!=MOTOR_ID_READY || m->fault || !m->result.valid ||
       c->drive_request || c->state!=SENSORLESS_STOP || c->fault) return 0u;
    return sensorless_control_set_motor_params(c,m->result.r_uohm,
                                               m->result.l_nh,m->result.flux_uwb);
}
