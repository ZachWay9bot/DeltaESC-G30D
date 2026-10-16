#include "bemf_detector.h"

static uint32_t udiv32(uint32_t n, uint32_t d){
    if(!d) return 0u;
    uint32_t q=0u, r=0u;
    for(int i=31;i>=0;i--){
        r=(r<<1)|((n>>(uint32_t)i)&1u);
        if(r>=d){ r-=d; q|=(1u<<(uint32_t)i); }
    }
    return q;
}

static uint8_t sector_from_abc(uint16_t a,uint16_t b,uint16_t c){
    if(a>=b){
        if(b>=c) return 0u;
        if(a>=c) return 5u;
        return 4u;
    }
    if(a>=c) return 1u;
    if(b>=c) return 2u;
    return 3u;
}

void bemf_detector_init(bemf_detector_t *d,uint16_t threshold_span){
    d->threshold_span=threshold_span;
    d->span=0u; d->common=0u; d->sector=BEMF_NO_SECTOR; d->last_sector=BEMF_NO_SECTOR;
    d->direction=0; d->direction_score=0; d->transitions=0u; d->bad_jumps=0u;
    d->last_transition_cycles=0u; d->avg_transition_cycles=0u;
}

void bemf_detector_reset_stats(bemf_detector_t *d){
    uint16_t t=d->threshold_span;
    bemf_detector_init(d,t);
}

void bemf_detector_update(bemf_detector_t *d,uint16_t a,uint16_t b,uint16_t c,uint32_t now){
    uint16_t hi=a, lo=a;
    if(b>hi) hi=b;
    if(c>hi) hi=c;
    if(b<lo) lo=b;
    if(c<lo) lo=c;
    d->span=(uint16_t)(hi-lo);
    d->common=(uint16_t)(((uint32_t)a+(uint32_t)b+(uint32_t)c)/3u);
    if(d->span<d->threshold_span){ d->sector=BEMF_NO_SECTOR; return; }

    uint8_t s=sector_from_abc(a,b,c);
    d->sector=s;
    if(d->last_sector==BEMF_NO_SECTOR){ d->last_sector=s; return; }
    if(s==d->last_sector) return;

    uint8_t step=(uint8_t)((s+6u-d->last_sector)%6u);
    if(step==1u || step==5u){
        int8_t sign=(step==1u)?1:-1;
        if(sign>0){ if(d->direction_score<6)d->direction_score++; }
        else { if(d->direction_score>-6)d->direction_score--; }
        if(d->direction_score>=3)d->direction=1;
        else if(d->direction_score<=-3)d->direction=-1;
        else d->direction=0;

        if(d->last_transition_cycles){
            uint32_t dt=now-d->last_transition_cycles;
            if(dt>0u){
                if(!d->avg_transition_cycles)d->avg_transition_cycles=dt;
                else d->avg_transition_cycles=(d->avg_transition_cycles*7u+dt)>>3;
            }
        }
        d->last_transition_cycles=now;
        d->transitions++;
    } else {
        d->bad_jumps++;
        d->direction_score=0;
        d->direction=0;
        d->last_transition_cycles=now;
        d->avg_transition_cycles=0u;
    }
    d->last_sector=s;
}

uint32_t bemf_electrical_hz_x10(const bemf_detector_t *d,uint32_t cpu_hz){
    if(!d->avg_transition_cycles) return 0u;
    uint32_t denom=d->avg_transition_cycles;
    if(denom>0x2AAAAAAAu) return 0u;
    denom*=6u;
    uint32_t numer=cpu_hz;
    if(numer<=0x19999999u) numer*=10u;
    else return udiv32(cpu_hz,denom)*10u;
    return udiv32(numer,denom);
}

uint32_t bemf_mechanical_rpm_x10(const bemf_detector_t *d,uint32_t cpu_hz,uint32_t pole_pairs){
    if(!pole_pairs) return 0u;
    uint32_t ehz10=bemf_electrical_hz_x10(d,cpu_hz);
    return udiv32(ehz10*60u,pole_pairs);
}
