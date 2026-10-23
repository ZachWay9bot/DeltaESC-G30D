#pragma once
#include <stdint.h>

/* Software decisions shared by target integration and native C tests.
 * No peripheral access, motor model, or IAP permission is implemented here. */
uint16_t drive_start_blockers(uint16_t commissioning_mask, uint8_t rolling_rc);
uint8_t drive_armed_owner_valid(uint8_t id_allowed, uint8_t id_requested,
    uint8_t id_owner, uint8_t id_state_valid, uint8_t drive_allowed,
    uint8_t pipeline_powered, uint8_t drive_request);
int16_t drive_throttle_counts(uint8_t throttle, uint8_t brake,
                             uint8_t idle_max, int16_t ceiling_counts);
uint16_t drive_speed_kmh10(int16_t electrical_step, uint8_t pole_pairs,
                          uint16_t circumference_mm);
