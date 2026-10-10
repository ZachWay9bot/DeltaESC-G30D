/* Host-only model test for the adapted VESC Ortega / EBiCS integer flux observer.
 * The synthetic voltage follows an ideal permanent-magnet flux derivative,
 * with no load or sensor offsets. No physical hardware validated. */
#include <math.h>
#include <stdio.h>
#include <stdint.h>
#include "vesc_ebics_flux.h"
#define PI 3.14159265358979323846
#define REQ(x) do { if(!(x)) { fprintf(stderr,"FAIL %s:%d: %s\n",__FILE__,__LINE__,#x); return 1; } }while(0)
int main(void) {
    vesc_ebics_flux_t f={0};
    const double lambda=0.015, speed=80.0;
    int good=0,bad=0;
    for(int i=0;i<80000;i++) {
        const double t=(double)i/4000.0, angle=t*speed;
        const int32_t va=(int32_t)lround(-speed*lambda*sin(angle)*1000.0);
        const int32_t vb=(int32_t)lround( speed*lambda*cos(angle)*1000.0);
        REQ(vesc_ebics_flux_step(&f,va,vb,0,0,100000u,100000u,15000u));
        if (i>20000 && i%100==0) {
            const double observed=atan2((double)f.flux_beta_nwb,(double)f.flux_alpha_nwb);
            const double error=remainder(observed-angle,2.0*PI);
            if(fabs(error)<(20.0*PI/180.0)) good++; else bad++;
        }
    }
    fprintf(stderr,"flux observer angle within 20 degrees: %d/%d\n",good,good+bad);
    REQ(good>450 && bad<150);
    REQ(f.valid);
    REQ(!vesc_ebics_flux_step(&f,0,0,0,0,0,100000u,15000u));
    REQ(!f.valid);
    vesc_ebics_flux_reset(&f);
    REQ(!f.samples && !f.x_alpha_nwb && !f.x_beta_nwb && !f.valid);
    puts("PASS VESC Ortega / EBiCS fixed-point observer: synthetic flux phase, reset, invalid config");
    return 0;
}