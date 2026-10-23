#pragma once
#include <stdint.h>
#include "ninebot_link.h"

#define G30_ESC_ADDR          0x20u
#define G30_DASH_ADDR         0x21u
#define G30_DASH_CMD_STATUS   0x64u
#define G30_DASH_CMD_ADC      0x65u
#define G30_DASH_STATUS_BYTES 6u

/* Decode the stock dashboard ADC report. The G30 payload layout used by the
 * proven VESC/dashboard bridge stores throttle/brake in payload[1]/[2]. */
uint8_t dashboard_decode_adc(const ninebot_frame_t *f, uint8_t *throttle, uint8_t *brake);

/* True for the native G30 0x64 request that expects an ESC status response. */
uint8_t dashboard_is_status_request(const ninebot_frame_t *f);

/* First-hardware-test status payload. Values are deliberately non-driving:
 * Drive icon, neutral 50% placeholder battery, lights/beep/speed/fault zero. */
void dashboard_safe_status_payload(uint8_t out[G30_DASH_STATUS_BYTES]);
