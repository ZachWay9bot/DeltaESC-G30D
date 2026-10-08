#include "commissioning_guard.h"

uint16_t commissioning_guard_eval(const commissioning_guard_input_t *in) {
    uint16_t m=0u;
    if ((uint32_t)(in->now_ms-in->adc_last_ms) > in->adc_stale_ms) m|=COMM_GUARD_ADC_STALE;
    if (in->abs_current_counts > in->idle_current_limit_counts) m|=COMM_GUARD_CURRENT_NOT_IDLE;
    if (!in->offset_valid) m|=COMM_GUARD_OFFSET_INVALID;
    if (!in->params_valid) m|=COMM_GUARD_PARAMS_INVALID;
    if (!in->dash_seen || (uint32_t)(in->now_ms-in->dash_last_ms) > in->dash_stale_ms) m|=COMM_GUARD_DASH_STALE;
    if (in->throttle > in->input_idle_max) m|=COMM_GUARD_THROTTLE_NOT_IDLE;
    if (in->brake > in->input_idle_max) m|=COMM_GUARD_BRAKE_NOT_IDLE;
    if (in->safety_latched) m|=COMM_GUARD_SAFETY_LATCHED;
    if (in->iap_busy) m|=COMM_GUARD_IAP_BUSY;
    if (in->poweroff_pending) m|=COMM_GUARD_POWEROFF_PENDING;
    if (!in->control_stopped) m|=COMM_GUARD_CONTROL_NOT_STOPPED;
    if (in->vbus_raw < in->vbus_raw_min || in->vbus_raw > in->vbus_raw_max) m|=COMM_GUARD_VBUS_RAW_INVALID;
    if (!in->current_scale_hw_valid) m|=COMM_GUARD_CURRENT_SCALE_HW;
    if (!in->gate_hw_valid) m|=COMM_GUARD_GATE_HW;
    if (!in->adc_timing_hw_valid) m|=COMM_GUARD_ADC_TIMING_HW;
    return m;
}
