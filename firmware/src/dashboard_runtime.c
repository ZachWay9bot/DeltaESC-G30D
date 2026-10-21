#include "dashboard_runtime.h"

static uint8_t native_dash_to_esc(const ninebot_frame_t *f) {
    return (uint8_t)(f != 0 &&
                     f->format == NINEBOT_FMT_5AA5 &&
                     f->src == G30_DASH_ADDR &&
                     f->dst == G30_ESC_ADDR);
}

uint8_t dashboard_decode_adc(const ninebot_frame_t *f, uint8_t *throttle, uint8_t *brake) {
    if (!native_dash_to_esc(f) || f->cmd != G30_DASH_CMD_ADC ||
        f->payload_len < 3u || throttle == 0 || brake == 0) {
        return 0u;
    }
    *throttle = f->payload[1];
    *brake = f->payload[2];
    return 1u;
}

uint8_t dashboard_is_status_request(const ninebot_frame_t *f) {
    return (uint8_t)(native_dash_to_esc(f) && f->cmd == G30_DASH_CMD_STATUS);
}

void dashboard_safe_status_payload(uint8_t out[G30_DASH_STATUS_BYTES]) {
    if (!out) return;
    out[0] = 0x01u; /* DRIVE icon */
    out[1] = 50u;   /* explicit bench placeholder; vbus scaling still deferred */
    out[2] = 0u;    /* light */
    out[3] = 0u;    /* beep */
    out[4] = 0u;    /* speed km/h */
    out[5] = 0u;    /* dashboard-visible fault */
}

void dashboard_drive_status_payload(uint8_t out[G30_DASH_STATUS_BYTES],
    uint16_t speed,uint8_t battery,uint8_t fault) {
    if(!out)return;
    uint32_t whole_kmh=((uint32_t)speed+5u)/10u;
    out[0]=0x01u;out[1]=battery>100u?100u:battery;
    out[2]=0u;out[3]=0u;
    out[4]=(uint8_t)(whole_kmh>255u?255u:whole_kmh);out[5]=fault;
}
