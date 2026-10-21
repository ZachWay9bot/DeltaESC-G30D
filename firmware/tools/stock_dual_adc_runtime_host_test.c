#include <stdint.h>
#include <stdio.h>
#include "stock_dual_adc_runtime.h"
#include "stock_dual_adc_plan.h"
#define T(x) do{if(!(x)){printf("FAIL at %d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(void){
    const uint16_t offsets[3]={2000u,2050u,2100u};
    stock_dual_adc_runtime_t s;stock_dual_adc_runtime_init(&s,offsets);
    T(s.sector==1u&&!s.offset_valid);
    stock_dual_adc_runtime_begin_offset(&s);
    for(unsigned i=0u;i<768u;i++){
        stock_adc_pair_t pair=stock_current_pair_for_sector(s.sector);
        T(pair.adc1_channel!=pair.adc2_channel);
        uint16_t x=offsets[pair.adc1_channel-3u],y=offsets[pair.adc2_channel-3u];
        uint8_t ended=stock_dual_adc_runtime_capture(&s,x,y);
        if(i<767u)T(!ended);
        if(i==767u)T(ended);
        stock_dual_adc_runtime_next_sector(&s,stock_dual_adc_runtime_rotate(s.sector));
    }
    T(s.offset_valid&&!s.calibrating);
    for(unsigned i=0;i<3;i++)T(s.cal_samples[i]==512u&&s.offsets[i]==offsets[i]);
    for(uint8_t sector=1;sector<=6;sector++){
        stock_dual_adc_runtime_next_sector(&s,sector);
        stock_adc_pair_t pair=stock_current_pair_for_sector(s.sector);
        uint16_t ra=offsets[pair.adc1_channel-3u]+(uint16_t)(sector&1u);
        uint16_t rb=offsets[pair.adc2_channel-3u]+(uint16_t)((sector>>1)&1u);
        T(!stock_dual_adc_runtime_capture(&s,ra,rb));
        stock_phase_currents_t c=stock_dual_adc_runtime_currents(&s,ra,rb);
        T(c.ia+c.ib+c.ic==0);
        stock_dual_adc_plan_t p=stock_dual_adc_plan_init(sector);
        T(p.adc1_channel==pair.adc1_channel&&p.adc2_channel==pair.adc2_channel);
    }
    stock_dual_adc_runtime_begin_offset(&s);
    stock_dual_adc_runtime_capture(&s,2040u,2140u);
    stock_dual_adc_runtime_abort_offset(&s);
    T(!s.offset_valid&&!s.calibrating);
    puts("PASS v0.8.5: six sectors, simultaneous pairs, 512/ch calibration, abort, zero-sum reconstruction");
}
