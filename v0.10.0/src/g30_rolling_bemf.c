#include "g30_rolling_bemf.h"

static uint8_t sector_from_abc(uint16_t a,uint16_t b,uint16_t c){
    if(a>=b){if(b>=c)return 0u;if(a>=c)return 5u;return 4u;}
    if(a>=c)return 1u;
    if(b>=c)return 2u;
    return 3u;
}

void g30_rolling_bemf_default_config(g30_rolling_bemf_config_t *c){
    if(!c)return;
    c->sysclk_hz=64000000u;
    c->control_hz=4000u;
    c->wheel_circ_mm=763u; /* 243 mm tire assumption from FO70: hardware-calibrate */
    c->pole_pairs=15u;     /* G30 assumption: hardware-calibrate */
    c->forward_raw_step=5u; /* FO69/FO70 proven XBOT physical-forward convention */
    c->min_speed_kmh10=30u;
    c->min_bemf_span=40u;
    c->min_forward_edges=4u;
    c->max_edge_age_cycles=(64000000u/1000u)*70u;
    c->max_sample_age_cycles=(64000000u/1000u)*20u;
    c->min_edge_cycles=50000u;
    c->max_edge_cycles=64000000u/8u;
    c->phase_offset_u16=0;
}

static void lose_lock(g30_rolling_bemf_t *m){
    m->forward_edges=0u;
    m->direction_forward=0u;
    m->speed_kmh10=0u;
    m->avg_edge_cycles=0u;
    m->electrical_step_u16=0;
}

void g30_rolling_bemf_init(g30_rolling_bemf_t *m,const g30_rolling_bemf_config_t *c){
    if(!m)return;
    g30_rolling_bemf_config_t d;
    if(!c){g30_rolling_bemf_default_config(&d);c=&d;}
    m->cfg=*c;
    g30_rolling_bemf_reset(m);
}

void g30_rolling_bemf_reset(g30_rolling_bemf_t *m){
    if(!m)return;
    m->a=m->b=m->c=m->span=m->speed_kmh10=0u;
    m->forward_edges=m->reverse_edges=m->bad_jumps=0u;
    m->last_edge_cycles=m->last_sample_cycles=m->avg_edge_cycles=0u;
    m->phase_u16=0u;m->electrical_step_u16=0;
    m->last_sector=G30_ROLL_NO_SECTOR;m->direction_forward=0u;m->samples=0u;
}

static uint16_t edge_speed_kmh10(const g30_rolling_bemf_config_t *c,uint32_t dt){
    if(!c || !dt || !c->pole_pairs || !c->wheel_circ_mm)return 0u;
    /* 10*3.6*C[m]*clk/(6*pole_pairs*dt). Rearranged to stay 32-bit for G30. */
    const uint32_t clk_khz=c->sysclk_hz/1000u;
    if(!clk_khz)return 0u;
    const uint32_t a=36u*(uint32_t)c->wheel_circ_mm;
    if(a>0xffffffffu/clk_khz)return 0u;
    const uint32_t num=(a*clk_khz)/(6u*(uint32_t)c->pole_pairs);
    uint32_t v=num/dt;
    return (uint16_t)(v>65535u?65535u:v);
}

static int16_t step_per_control(const g30_rolling_bemf_config_t *c,uint32_t edge_cycles,uint8_t raw_step){
    if(!c || !edge_cycles || !c->control_hz)return 0;
    const uint32_t cpt=c->sysclk_hz/(uint32_t)c->control_hz;
    uint32_t mag=(G30_ROLL_PHASE60_U16*cpt + edge_cycles/2u)/edge_cycles;
    if(mag>32767u)mag=32767u;
    /* XBOT FO69/FO70: raw step 5 is physical forward and negative electrical rotation. */
    return raw_step==5u?-(int16_t)mag:(int16_t)mag;
}

void g30_rolling_bemf_sample(g30_rolling_bemf_t *m,uint16_t a,uint16_t b,uint16_t c,uint32_t now){
    if(!m)return;
    uint16_t hi=a,lo=a;
    if(b>hi)hi=b;
    if(c>hi)hi=c;
    if(b<lo)lo=b;
    if(c<lo)lo=c;
    m->a=a;m->b=b;m->c=c;m->span=(uint16_t)(hi-lo);m->last_sample_cycles=now;
    if(m->samples<65535u)m->samples++;
    if(m->span<m->cfg.min_bemf_span){lose_lock(m);m->last_sector=G30_ROLL_NO_SECTOR;m->last_edge_cycles=0u;return;}
    const uint8_t s=sector_from_abc(a,b,c);
    m->phase_u16=(uint16_t)((uint32_t)s*G30_ROLL_PHASE60_U16+(uint16_t)m->cfg.phase_offset_u16);
    if(m->last_sector==G30_ROLL_NO_SECTOR){m->last_sector=s;m->last_edge_cycles=now;return;}
    if(m->last_edge_cycles && (uint32_t)(now-m->last_edge_cycles)>m->cfg.max_edge_age_cycles){lose_lock(m);m->last_sector=s;m->last_edge_cycles=now;return;}
    if(s==m->last_sector)return;
    const uint8_t step=(uint8_t)((s+6u-m->last_sector)%6u);
    const uint32_t dt=now-m->last_edge_cycles;
    m->last_sector=s;m->last_edge_cycles=now;
    if(step!=1u && step!=5u){if(m->bad_jumps<65535u)m->bad_jumps++;lose_lock(m);return;}
    if(step!=m->cfg.forward_raw_step){if(m->reverse_edges<65535u)m->reverse_edges++;lose_lock(m);return;}
    if(dt<m->cfg.min_edge_cycles || dt>m->cfg.max_edge_cycles){lose_lock(m);return;}
    const uint16_t v=edge_speed_kmh10(&m->cfg,dt);
    if(v>400u){lose_lock(m);return;}
    if(m->speed_kmh10){
        const uint32_t prev=m->speed_kmh10;
        if((uint32_t)v*2u<prev || (uint32_t)v>prev*2u){lose_lock(m);return;}
        m->speed_kmh10=(uint16_t)((3u*prev+v+2u)/4u);
    }else m->speed_kmh10=v;
    m->avg_edge_cycles=m->avg_edge_cycles?((m->avg_edge_cycles*7u+dt)>>3):dt;
    m->electrical_step_u16=step_per_control(&m->cfg,m->avg_edge_cycles,step);
    if(m->forward_edges<65535u)m->forward_edges++;
    if(m->forward_edges>=m->cfg.min_forward_edges)m->direction_forward=1u;
}

uint16_t g30_rolling_bemf_speed_kmh10(const g30_rolling_bemf_t *m,uint32_t now){
    if(!m || m->last_sector==G30_ROLL_NO_SECTOR || !m->last_edge_cycles || !m->forward_edges)return 0u;
    if((uint32_t)(now-m->last_edge_cycles)>m->cfg.max_edge_age_cycles)return 0u;
    if((uint32_t)(now-m->last_sample_cycles)>m->cfg.max_sample_age_cycles)return 0u;
    return m->speed_kmh10;
}

uint8_t g30_rolling_bemf_ready(const g30_rolling_bemf_t *m,uint32_t now){
    if(!m)return 0u;
    return (uint8_t)(m->direction_forward && m->forward_edges>=m->cfg.min_forward_edges &&
                     m->span>=m->cfg.min_bemf_span && m->electrical_step_u16<0 &&
                     g30_rolling_bemf_speed_kmh10(m,now)>=m->cfg.min_speed_kmh10);
}
