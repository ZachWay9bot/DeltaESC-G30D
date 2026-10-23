#pragma once
#include <stdint.h>
#define BLE_PROBE_VERSION 1u
/* Read-only Ninebot ESC registers 0xDA..0xDF, 16-byte payload each. */
uint8_t ble_motor_probe_read(uint8_t reg, uint8_t out[16]);
