#pragma once
#include <stdint.h>

#define G30_ROLL_NO_SECTOR 0xFFu
#define G30_ROLL_PHASE60_U16 10923u

typedef struct {
    uint32_t sysclk_hz;
    uint16_t control_hz;
    uint16_t wheel_circ_mm;
    uint8_t pole_pairs;
    uint8_t forward_raw_step; /* XBOT FO69/FO70 physical-forward convention: 5 */
    uint16_t min_speed_kmh10;
    uint16_t min_bemf_span;
    uint8_t min_forward_edges;
    uint32_t max_edge_age_cycles;
    uint32_t max_sample_age_cycles;
    uint32_t min_edge_cycles;
    uint32_t max_edge_cycles;
    int16_t phase_offset_u16; /* optional G30 wiring/calibration correction after BEMF->flux +90deg */
} g30_rolling_bemf_config_t;

typedef struct {
    g30_rolling_bemf_config_t cfg;
    uint16_t a,b,c,span;
    uint16_t speed_kmh10;
    uint16_t forward_edges,reverse_edges,bad_jumps;
    uint32_t last_edge_cycles,last_sample_cycles,avg_edge_cycles;
    uint16_t phase_u16; /* estimated PM rotor-flux phase, not raw BEMF rank angle */
    int16_t electrical_step_u16;
    uint8_t last_sector;
    uint8_t direction_forward;
    uint16_t samples;
} g30_rolling_bemf_t;

void g30_rolling_bemf_default_config(g30_rolling_bemf_config_t *c);
void g30_rolling_bemf_init(g30_rolling_bemf_t *m,const g30_rolling_bemf_config_t *c);
void g30_rolling_bemf_reset(g30_rolling_bemf_t *m);
void g30_rolling_bemf_sample(g30_rolling_bemf_t *m,uint16_t a,uint16_t b,uint16_t c,uint32_t now_cycles);
uint8_t g30_rolling_bemf_ready(const g30_rolling_bemf_t *m,uint32_t now_cycles);
uint16_t g30_rolling_bemf_speed_kmh10(const g30_rolling_bemf_t *m,uint32_t now_cycles);
