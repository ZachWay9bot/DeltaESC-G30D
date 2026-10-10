#include "motor_id_auto.h"
#include <stddef.h>

/* Modest voltage requests: hardware adapter must enforce stricter limits,
 * bound actual voltage/current and validate each measurement independently.
 * Do not interpret these as safe limits for an uncalibrated controller. */
#define AUTO_R_START_MV 100
#define AUTO_R_STEP_MV 50
#define AUTO_R_MAX_MV 750
#define AUTO_R_TARGET_MIN_MA 600
#define AUTO_R_TARGET_MAX_MA 1200
#define AUTO_L_MV 1000
#define AUTO_FLUX_MV 500
#define AUTO_R_SETTLE_US 8000u
#define AUTO_R_INTERVAL_US 1000u
#define AUTO_L_REST_US 2000u
#define AUTO_L_PULSE_US 1000u
#define AUTO_FLUX_HOLD_US 100000u
#define AUTO_FLUX_INTERVAL_US 1000u
#define AUTO_FLUX_SPEED_MRAD_S 10000u
#define AUTO_CONTROL_GAP_MAX_US 50000u
#define AUTO_R_DEADLINE_US 100000u
#define AUTO_L_DEADLINE_US 200000u
#define AUTO_FLUX_DEADLINE_US 2800000u

static uint8_t running(uint8_t s) {
    return s>=ID_AUTO_R_SETTLE && s<=ID_AUTO_FLUX_COAST;
}
static uint8_t fault(motor_id_auto_t *a, uint8_t why) {
    if(a->io.gate_off) a->io.gate_off(a->io_ctx);
    a->output_active=0u;
    a->fault=(uint8_t)(a->fault|why);
    motor_id_abort(&a->id);
    a->state=ID_AUTO_FAULT;
    return 0u;
}
static uint8_t output(motor_id_auto_t *a, uint32_t now_us, int32_t mv, uint16_t phase) {
    if(!a->io.vector_mv || !a->io.vector_mv(a->io_ctx,mv,phase))
        return fault(a,ID_AUTO_FAULT_DRIVER);
    a->output_active=1u;
    a->command_epoch++;
    a->command_at_us=now_us;
    return 1u;
}
static void coast(motor_id_auto_t *a) {
    a->io.gate_off(a->io_ctx);
    a->output_active=0u;
    a->command_epoch++;
}
static uint8_t collect(motor_id_auto_t *a, uint32_t dt,
                       const motor_id_auto_feedback_t *fb) {
    motor_id_sample_t s={0};
    if(fb->command_epoch!=a->command_epoch || !fb->adc_valid || !fb->voltage_valid ||
       (a->id.state==MOTOR_ID_FLUX && (!fb->bemf_valid || !fb->speed_valid)))
        return fault(a,ID_AUTO_FAULT_SENSOR);
    s.sequence=++a->seq;
    s.elapsed_us=dt;
    s.samples_synchronized=1u;
    s.stage=a->id.state;
    s.voltage_mv=fb->applied_voltage_mv;
    s.current_ma=fb->phase_current_ma;
    s.previous_current_ma=a->previous_current_ma;
    s.bemf_mv=fb->measured_bemf_mv;
    s.electrical_speed_mrad_s=fb->measured_electrical_speed_mrad_s;
    if(!motor_id_accept(&a->id,&s)) return fault(a,ID_AUTO_FAULT_SENSOR);
    return 1u;
}
void motor_id_auto_init(motor_id_auto_t *a, motor_id_auto_io_t io,
                        void *ctx, sensorless_control_t *control) {
    if(!a)return;
    volatile uint8_t *bytes=(volatile uint8_t*)a;
    for(uint32_t k=0;k<sizeof(*a);k++)bytes[k]=0u;
    a->io=io;a->io_ctx=ctx;a->control=control;
    a->state=ID_AUTO_IDLE;
    motor_id_init(&a->id);
}
void motor_id_auto_bind_config(motor_id_auto_t *a, motor_config_txn_t *config) {
    if(a && a->state==ID_AUTO_IDLE) a->config=config;
}

static void copy_struct(void *dst, const void *src, uint32_t count) {
    volatile uint8_t *d=(volatile uint8_t*)dst;
    const volatile uint8_t *s=(const volatile uint8_t*)src;
    for(uint32_t k=0;k<count;k++)d[k]=s[k];
}

static uint8_t adopt(motor_id_auto_t *a) {
    /* Prepare the new sensorless model and configuration entirely off-line.
     * Either both RAM objects publish or neither does. Never write flash. */
    sensorless_control_t staged_control;
    copy_struct(&staged_control,a->control,sizeof(staged_control));
    if(!motor_id_commit(&a->id,&staged_control))return 0u;
    if(a->config) {
        motor_config_txn_t staged_config;
        copy_struct(&staged_config,a->config,sizeof(staged_config));
        if(staged_config.pending_mask)return 0u;
        if(!motor_config_stage_u32(&staged_config,0xF0u,a->id.result.r_uohm) ||
           !motor_config_stage_u32(&staged_config,0xF1u,a->id.result.l_nh) ||
           !motor_config_stage_u32(&staged_config,0xF2u,a->id.result.flux_uwb) ||
           !motor_config_stage_i16(&staged_config,staged_config.active.phase_offset) ||
           !motor_config_stage_u16(&staged_config,staged_config.active.test_current_ma) ||
           !motor_config_commit(&staged_config))return 0u;
        copy_struct(a->config,&staged_config,sizeof(staged_config));
    }
    copy_struct(a->control,&staged_control,sizeof(staged_control));
    if(a->io.result_applied)a->io.result_applied(a->io_ctx,&a->id.result);
    return 1u;
}

uint8_t motor_id_auto_begin(motor_id_auto_t *a,
                            const motor_id_qualification_t *q,
                            uint32_t now_us) {
    if(!a || !a->io.gate_off || !a->io.vector_mv || !a->control ||
       a->state!=ID_AUTO_IDLE || a->control->drive_request ||
       a->control->state!=SENSORLESS_STOP || a->control->fault ||
       (a->config && a->config->pending_mask))
        return 0u;
    /* Establish hardware-coast BEFORE accepting any power qualification. */
    coast(a);
    if(!motor_id_begin(&a->id,q)) {
        a->state=ID_AUTO_FAULT; a->fault=ID_AUTO_FAULT_SAFETY;
        return 0u;
    }
    a->last_event_us=now_us;
    a->stage_started_us=now_us;
    a->next_sample_us=now_us+AUTO_R_SETTLE_US;
    a->state=ID_AUTO_R_SETTLE;
    a->r_request_mv=AUTO_R_START_MV;
    if(!output(a,now_us,a->r_request_mv,0u))return 0u;
    return 1u;
}
void motor_id_auto_abort(motor_id_auto_t *a) {
    if(!a)return;
    (void)fault(a,ID_AUTO_FAULT_SAFETY);
}
uint8_t motor_id_auto_tick(motor_id_auto_t *a, uint32_t now_us,
                           const motor_id_auto_feedback_t *fb) {
    if(!a || a->state==ID_AUTO_FAULT || a->state==ID_AUTO_IDLE) return 0u;
    if(a->state==ID_AUTO_COMPLETE)return 1u;
    if(!running(a->state) || !fb || fb->stop_requested || fb->brake_active ||
       fb->fault_active || !fb->throttle_idle ||
       !a->control || a->control->drive_request ||
       a->control->state!=SENSORLESS_STOP || a->control->fault ||
       (a->config && a->config->pending_mask))
        return fault(a,ID_AUTO_FAULT_SAFETY);
    uint32_t delta=now_us-a->last_event_us;
    if(delta==0u || delta>AUTO_CONTROL_GAP_MAX_US)
        return fault(a,ID_AUTO_FAULT_WATCHDOG);
    /* Fail in every excitation phase, not only at tuple collection. */
    if(!fb->adc_valid || fb->command_epoch!=a->command_epoch ||
       fb->phase_current_ma>2000 || fb->phase_current_ma< -2000)
        return fault(a,ID_AUTO_FAULT_SENSOR);
    a->last_event_us=now_us;
    uint32_t stage_age=now_us-a->stage_started_us;
    if((a->state<=ID_AUTO_R_COLLECT && stage_age>AUTO_R_DEADLINE_US) ||
       (a->state>=ID_AUTO_L_REST && a->state<=ID_AUTO_L_PULSE && stage_age>AUTO_L_DEADLINE_US) ||
       (a->state>=ID_AUTO_FLUX_SPIN && stage_age>AUTO_FLUX_DEADLINE_US))
        return fault(a,ID_AUTO_FAULT_WATCHDOG);
    switch(a->state) {
    case ID_AUTO_R_SETTLE:
        /* Controlled stepwise excitation as in VESC R detect: never jump
         * straight to full voltage on an unknown winding resistance. */
        if(!fb->voltage_valid || fb->applied_voltage_mv<0 ||
           fb->applied_voltage_mv>a->r_request_mv+20 ||
           fb->phase_current_ma<0 ||
           fb->phase_current_ma>AUTO_R_TARGET_MAX_MA)
            return fault(a,ID_AUTO_FAULT_SENSOR);
        if(fb->phase_current_ma<AUTO_R_TARGET_MIN_MA) {
            if(a->r_request_mv>=AUTO_R_MAX_MV)break;
            a->r_request_mv+=AUTO_R_STEP_MV;
            if(!output(a,now_us,a->r_request_mv,0u))return 0u;
            a->next_sample_us=now_us+AUTO_R_SETTLE_US;
        } else if((int32_t)(now_us-a->next_sample_us)>=0) {
            a->state=ID_AUTO_R_COLLECT;
            a->next_sample_us=now_us;
        }
        break;
    case ID_AUTO_R_COLLECT:
        if(!fb->voltage_valid || fb->applied_voltage_mv<=0 ||
           fb->applied_voltage_mv>a->r_request_mv+20 ||
           fb->phase_current_ma<AUTO_R_TARGET_MIN_MA ||
           fb->phase_current_ma>AUTO_R_TARGET_MAX_MA)
            return fault(a,ID_AUTO_FAULT_SENSOR);
        if((int32_t)(now_us-a->next_sample_us)>=0) {
            if(!collect(a,AUTO_R_INTERVAL_US,fb))return 0u;
            a->next_sample_us=now_us+AUTO_R_INTERVAL_US;
            if(a->id.state==MOTOR_ID_INDUCTANCE) {
                coast(a);
                a->state=ID_AUTO_L_REST;
                a->stage_started_us=now_us;
                a->next_sample_us=now_us+AUTO_L_REST_US;
            }
        }
        break;
    case ID_AUTO_L_REST:
        if((int32_t)(now_us-a->next_sample_us)>=0) {
            if(!fb->voltage_valid || fb->applied_voltage_mv != 0 ||
               fb->phase_current_ma < -100 || fb->phase_current_ma > 100)
                return fault(a,ID_AUTO_FAULT_SENSOR);
            a->previous_current_ma=fb->phase_current_ma;
            if(!output(a,now_us,AUTO_L_MV,0u))return 0u;
            a->state=ID_AUTO_L_PULSE;
            a->next_sample_us=now_us+AUTO_L_PULSE_US;
        }
        break;
    case ID_AUTO_L_PULSE:
        if((int32_t)(now_us-a->next_sample_us)>=0) {
            const uint32_t pulse_us=now_us-a->command_at_us;
            if(pulse_us<AUTO_L_PULSE_US || pulse_us>AUTO_L_PULSE_US+250u)
                return fault(a,ID_AUTO_FAULT_WATCHDOG);
            if(!collect(a,pulse_us,fb))return 0u;
            coast(a);
            a->next_sample_us=now_us+AUTO_L_REST_US;
            if(a->id.state==MOTOR_ID_FLUX) {
                a->state=ID_AUTO_FLUX_SPIN;
                a->stage_started_us=now_us;
                a->next_sample_us=now_us+AUTO_FLUX_HOLD_US;
                a->flux_angle=0u;
                if(!output(a,now_us,AUTO_FLUX_MV,0u))return 0u;
            } else {
                a->state=ID_AUTO_L_REST;
            }
        }
        break;
    case ID_AUTO_FLUX_SPIN: {
        /* Commanded angle is only for excitation; NEVER use commanded
         * speed as a measured electrical speed in flux estimation. */
        /* 10 rad/s -> 104.303 turn16 counts/ms; no 64-bit divide. */
        const uint32_t dtheta=(delta*1043u)/10000u;
        a->flux_angle=(uint16_t)(a->flux_angle+(uint16_t)dtheta);
        if(!output(a,now_us,AUTO_FLUX_MV,a->flux_angle))return 0u;
        if((int32_t)(now_us-a->next_sample_us)>=0) {
            coast(a);
            a->state=ID_AUTO_FLUX_COAST;
            a->next_sample_us=now_us+AUTO_FLUX_INTERVAL_US;
        }
        break;
    }
    case ID_AUTO_FLUX_COAST:
        if((int32_t)(now_us-a->next_sample_us)>=0) {
            if(a->output_active)return fault(a,ID_AUTO_FAULT_DRIVER);
            if(!collect(a,AUTO_FLUX_INTERVAL_US,fb))return 0u;
            a->next_sample_us=now_us+AUTO_FLUX_INTERVAL_US;
            if(a->id.state==MOTOR_ID_READY) {
                /* Physical outputs OFF and model STOP before adopting R/L/flux. */
                coast(a);
                if(!adopt(a))
                    return fault(a,ID_AUTO_FAULT_COMMIT);
                a->state=ID_AUTO_COMPLETE;
            }
        }
        break;
    default:
        return fault(a,ID_AUTO_FAULT_SAFETY);
    }
    return 1u;
}