/* One-time offline edge tests for passive G30 roll qualification.
 * No fake hardware qualification, no ADC writes, no IAP changes. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "g30_rolling_bemf.h"
#include "drive_runtime.h"

static const uint16_t seq[6][3]={
    {3000,2000,1000},{2000,3000,1000},{1000,3000,2000},
    {1000,2000,3000},{2000,1000,3000},{3000,1000,2000}
};
static uint32_t feed(g30_rolling_bemf_t *m,uint32_t now,uint32_t dt,int backward) {
    for(unsigned k=0;k<9;k++){
        const unsigned ix=backward?k%6:(6-k%6)%6;
        g30_rolling_bemf_sample(m,seq[ix][0],seq[ix][1],seq[ix][2],now);
        now+=dt;
    }
    return now-dt;
}
static void init(g30_rolling_bemf_t *m,g30_rolling_bemf_config_t *cfg){
    g30_rolling_bemf_default_config(cfg);
    cfg->pole_pairs=15u;cfg->wheel_circ_mm=763u; /* geometry is HOST fixture only */
    g30_rolling_bemf_init(m,cfg);
}
int main(void){
    g30_rolling_bemf_t m;
    g30_rolling_bemf_config_t cfg;
    init(&m,&cfg);
    uint32_t last=feed(&m,1000000u,400000u,0);
    assert(g30_rolling_bemf_ready(&m,last+1000u));
    assert(m.electrical_step_u16<0);
    assert(m.speed_kmh10>=30u&&m.speed_kmh10<=60u);
    const uint16_t active=drive_speed_kmh10(m.electrical_step_u16,15u,763u);
    assert(active>=m.speed_kmh10-3u&&active<=m.speed_kmh10+3u);
    assert(!g30_rolling_bemf_ready(&m,last+cfg.max_sample_age_cycles+1u));
    assert(!g30_rolling_bemf_ready(&m,last+cfg.max_edge_age_cycles+1u));
    /* Wrong-direction movement must not be armed. */
    init(&m,&cfg);
    last=feed(&m,1000000u,400000u,1);
    assert(!g30_rolling_bemf_ready(&m,last+100u));
    /* Below 3km/h must not be armed, even with otherwise valid edges. */
    init(&m,&cfg);
    last=feed(&m,1000000u,1200000u,0);
    assert(!g30_rolling_bemf_ready(&m,last+100u));
    /* All phases equal is no BEMF and invalidates any earlier lock. */
    init(&m,&cfg);
    last=feed(&m,1000000u,400000u,0);
    assert(g30_rolling_bemf_ready(&m,last+100u));
    g30_rolling_bemf_sample(&m,2000,2000,2000,last+200u);
    assert(!g30_rolling_bemf_ready(&m,last+300u));
    /* The first hardware boot has unknown wheel geometry; no arm from that. */
    init(&m,&cfg);m.cfg.pole_pairs=0u;m.cfg.wheel_circ_mm=0u;
    last=feed(&m,1000000u,400000u,0);
    assert(!g30_rolling_bemf_ready(&m,last+100u));
    /* DWT_CYCCNT wrap must not lose forward direction with fresh samples. */
    init(&m,&cfg);
    last=feed(&m,0xfffff000u,400000u,0);
    assert(g30_rolling_bemf_ready(&m,last+100u));
    /* Invalid six-step jump cannot be mistaken for physical forward. */
    init(&m,&cfg);
    g30_rolling_bemf_sample(&m,seq[0][0],seq[0][1],seq[0][2],1000000u);
    g30_rolling_bemf_sample(&m,seq[3][0],seq[3][1],seq[3][2],1400000u);
    assert(m.bad_jumps>0u&&!g30_rolling_bemf_ready(&m,1400100u));
    /* Speed / pole-pair conversion stays bounded for all supported PP and geometry. */
    for(unsigned pp=1;pp<=64;pp++){
        for(unsigned circ=300;circ<=1800;circ+=50){
            for(int step=-1024;step<=1024;step+=32){
                const uint16_t speed=drive_speed_kmh10((int16_t)step,(uint8_t)pp,(uint16_t)circ);
                assert(speed==drive_speed_kmh10((int16_t)(step<0?-step:step),(uint8_t)pp,(uint16_t)circ));
            }
        }
    }
    assert(drive_speed_kmh10(-400,0,763)==0u);
    assert(drive_speed_kmh10(-400,15,0)==0u);
    assert(drive_speed_kmh10(-1025,15,763)==0u);
    puts("PASS one-shot G30 rolling edge cases: forward/reverse, 3 km/h, staleness, DWT wrap, noise, geometry and speed maths");
    return 0;
}
