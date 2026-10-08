#include <stdint.h>
#include <stdio.h>
#include "commissioning_guard.h"

static commissioning_guard_input_t good(void){
    commissioning_guard_input_t x={0};
    x.now_ms=1000; x.adc_last_ms=999; x.dash_last_ms=950;
    x.adc_stale_ms=5; x.dash_stale_ms=250;
    x.abs_current_counts=10; x.idle_current_limit_counts=120;
    x.vbus_raw=2000; x.vbus_raw_min=50; x.vbus_raw_max=4050;
    x.offset_valid=1; x.params_valid=1; x.dash_seen=1;
    x.throttle=0; x.brake=0; x.input_idle_max=3;
    x.control_stopped=1; x.current_scale_hw_valid=1; x.gate_hw_valid=1; x.adc_timing_hw_valid=1;
    return x;
}
static int expect(uint16_t got,uint16_t want,int code){if(got!=want){printf("got=%04x want=%04x\n",got,want);return code;}return 0;}
int main(void){
    commissioning_guard_input_t x=good(); int r=expect(commissioning_guard_eval(&x),0,1); if(r)return r;
#define T(field,val,bit,code) do{x=good();x.field=(val);r=expect(commissioning_guard_eval(&x),(bit),(code));if(r)return r;}while(0)
    T(adc_last_ms,990,COMM_GUARD_ADC_STALE,2);
    T(abs_current_counts,121,COMM_GUARD_CURRENT_NOT_IDLE,3);
    T(offset_valid,0,COMM_GUARD_OFFSET_INVALID,4);
    T(params_valid,0,COMM_GUARD_PARAMS_INVALID,5);
    T(dash_seen,0,COMM_GUARD_DASH_STALE,6);
    T(dash_last_ms,700,COMM_GUARD_DASH_STALE,7);
    T(throttle,4,COMM_GUARD_THROTTLE_NOT_IDLE,8);
    T(brake,4,COMM_GUARD_BRAKE_NOT_IDLE,9);
    T(safety_latched,1,COMM_GUARD_SAFETY_LATCHED,10);
    T(iap_busy,1,COMM_GUARD_IAP_BUSY,11);
    T(poweroff_pending,1,COMM_GUARD_POWEROFF_PENDING,12);
    T(control_stopped,0,COMM_GUARD_CONTROL_NOT_STOPPED,13);
    T(vbus_raw,49,COMM_GUARD_VBUS_RAW_INVALID,14);
    T(current_scale_hw_valid,0,COMM_GUARD_CURRENT_SCALE_HW,15);
    T(gate_hw_valid,0,COMM_GUARD_GATE_HW,16);
    T(adc_timing_hw_valid,0,COMM_GUARD_ADC_TIMING_HW,17);
    x=good();x.current_scale_hw_valid=0;x.gate_hw_valid=0;x.adc_timing_hw_valid=0;
    r=expect(commissioning_guard_eval(&x),COMM_GUARD_CURRENT_SCALE_HW|COMM_GUARD_GATE_HW|COMM_GUARD_ADC_TIMING_HW,18);if(r)return r;
    puts("commissioning guard host test: PASS");return 0;
}
