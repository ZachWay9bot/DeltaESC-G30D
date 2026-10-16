#pragma once
#include <stdint.h>

#define BEMF_NO_SECTOR 0xFFu

typedef struct {
    uint16_t threshold_span;
    uint16_t span;
    uint16_t common;
    uint8_t sector;
    uint8_t last_sector;
    int8_t direction;
    int8_t direction_score;
    uint32_t transitions;
    uint32_t bad_jumps;
    uint32_t last_transition_cycles;
    uint32_t avg_transition_cycles;
} bemf_detector_t;

void bemf_detector_init(bemf_detector_t *d, uint16_t threshold_span);
void bemf_detector_reset_stats(bemf_detector_t *d);
void bemf_detector_update(bemf_detector_t *d, uint16_t a, uint16_t b, uint16_t c, uint32_t now_cycles);
uint32_t bemf_electrical_hz_x10(const bemf_detector_t *d, uint32_t cpu_hz);
uint32_t bemf_mechanical_rpm_x10(const bemf_detector_t *d, uint32_t cpu_hz, uint32_t pole_pairs);
