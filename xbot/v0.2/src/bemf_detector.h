#pragma once
#include <stdint.h>
#define BEMF_NO_SECTOR 0xFFu
typedef struct{uint16_t threshold_span,span,common;uint8_t sector,last_sector;int8_t direction,direction_score;uint32_t transitions,bad_jumps,last_transition_cycles,avg_transition_cycles;} bemf_detector_t;
void bemf_detector_init(bemf_detector_t*,uint16_t);
void bemf_detector_reset_stats(bemf_detector_t*);
void bemf_detector_update(bemf_detector_t*,uint16_t,uint16_t,uint16_t,uint32_t);
uint32_t bemf_electrical_hz_x10(const bemf_detector_t*,uint32_t);
uint32_t bemf_mechanical_rpm_x10(const bemf_detector_t*,uint32_t,uint32_t);
