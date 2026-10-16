#include <stdint.h>
#include <stdio.h>
#include "bemf_detector.h"
static const uint16_t seq[6][3]={
 {300,200,100},{200,300,100},{100,300,200},
 {100,200,300},{200,100,300},{300,100,200}
};
int main(void){
 bemf_detector_t d;bemf_detector_init(&d,20);
 uint32_t t=100000;
 for(int round=0;round<8;round++)for(int s=0;s<6;s++){bemf_detector_update(&d,seq[s][0],seq[s][1],seq[s][2],t);t+=10000;}
 if(d.direction!=1)return 1;
 if(d.bad_jumps)return 2;
 bemf_detector_reset_stats(&d);t=100000;
 for(int round=0;round<8;round++)for(int s=5;s>=0;s--){bemf_detector_update(&d,seq[s][0],seq[s][1],seq[s][2],t);t+=10000;}
 if(d.direction!=-1)return 3;
 if(d.bad_jumps)return 4;
 bemf_detector_reset_stats(&d);
 bemf_detector_update(&d,2048,2049,2047,1000);
 if(d.sector!=BEMF_NO_SECTOR)return 5;
 bemf_detector_update(&d,300,200,100,2000);
 bemf_detector_update(&d,100,200,300,3000);
 if(d.bad_jumps!=1)return 6;
 puts("PASS: forward/reverse/noise/bad-jump BEMF detector");
 return 0;
}
