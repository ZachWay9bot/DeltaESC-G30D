#pragma once
#include <stdint.h>

typedef struct {
    uint8_t (*start_regular)(void *ctx,uint8_t channel);
    uint8_t (*poll_regular)(void *ctx,uint16_t *value); /* 1=complete, 0=pending */
    void (*abort_regular)(void *ctx);
} g30_passive_bemf_adc_io_t;

typedef struct {
    g30_passive_bemf_adc_io_t io;
    void *ctx;
    uint32_t next_start_ms;
    uint32_t started_ms;
    uint32_t period_ms;
    uint16_t sum[3];
    uint16_t out[3];
    uint8_t channel_index;
    uint8_t repeat_index;
    uint8_t pending;
    uint8_t fresh;
    uint16_t completed_frames;
    uint16_t timeouts;
} g30_passive_bemf_adc_t;

void g30_passive_bemf_adc_init(g30_passive_bemf_adc_t *s,g30_passive_bemf_adc_io_t io,void *ctx);
void g30_passive_bemf_adc_cancel(g30_passive_bemf_adc_t *s);
/* May be called only while bridge_armed=0. Never touches PWM or gate state itself. */
void g30_passive_bemf_adc_tick(g30_passive_bemf_adc_t *s,uint32_t now_ms,uint8_t bridge_armed,uint8_t regular_adc_busy);
uint8_t g30_passive_bemf_adc_take(g30_passive_bemf_adc_t *s,uint16_t out[3]);
