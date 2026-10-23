#pragma once
#include <stdint.h>

enum {
    COMM_GUARD_ADC_STALE             = 1u << 0,
    COMM_GUARD_CURRENT_NOT_IDLE      = 1u << 1,
    COMM_GUARD_OFFSET_INVALID        = 1u << 2,
    COMM_GUARD_PARAMS_INVALID        = 1u << 3,
    COMM_GUARD_DASH_STALE            = 1u << 4,
    COMM_GUARD_THROTTLE_NOT_IDLE     = 1u << 5,
    COMM_GUARD_BRAKE_NOT_IDLE        = 1u << 6,
    COMM_GUARD_SAFETY_LATCHED        = 1u << 7,
    COMM_GUARD_IAP_BUSY              = 1u << 8,
    COMM_GUARD_POWEROFF_PENDING      = 1u << 9,
    COMM_GUARD_CONTROL_NOT_STOPPED   = 1u << 10,
    COMM_GUARD_VBUS_RAW_INVALID      = 1u << 11,
    COMM_GUARD_CURRENT_SCALE_HW      = 1u << 12,
    COMM_GUARD_GATE_HW               = 1u << 13,
    COMM_GUARD_ADC_TIMING_HW         = 1u << 14
};

typedef struct {
    uint32_t now_ms;
    uint32_t adc_last_ms;
    uint32_t dash_last_ms;
    uint16_t adc_stale_ms;
    uint16_t dash_stale_ms;
    uint16_t abs_current_counts;
    uint16_t idle_current_limit_counts;
    uint16_t vbus_raw;
    uint16_t vbus_raw_min;
    uint16_t vbus_raw_max;
    uint8_t offset_valid;
    uint8_t params_valid;
    uint8_t dash_seen;
    uint8_t throttle;
    uint8_t brake;
    uint8_t input_idle_max;
    uint8_t safety_latched;
    uint8_t iap_busy;
    uint8_t poweroff_pending;
    uint8_t control_stopped;
    uint8_t current_scale_hw_valid;
    uint8_t gate_hw_valid;
    uint8_t adc_timing_hw_valid;
} commissioning_guard_input_t;

uint16_t commissioning_guard_eval(const commissioning_guard_input_t *in);
