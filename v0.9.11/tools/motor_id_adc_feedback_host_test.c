#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "motor_id_adc_feedback.h"
#include "stock_current_frontend.h"

#define CHECK(x) do { if(!(x)){fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x);exit(1);} }while(0)
static motor_id_adc_feedback_t f;
static motor_id_auto_feedback_t out;
static motor_id_adc_input_t in;
static void setup(void){
    motor_id_adc_feedback_init(&f);
    in=(motor_id_adc_input_t){0};
    in.clock_hz=64000000u;in.now_cycles=200000;
    in.vbus_mv=36000;in.tim1_arr=4000;
    in.offset[0]=2048;in.offset[1]=2048;in.offset[2]=2048;
    in.adc2_ch4_offset=2048;in.adc2_ch5_offset=2048;
    in.adc2_ch4_offset_valid=1;in.adc2_ch5_offset_valid=1;
    in.adc1_jeoc=1;in.adc2_jeoc=1;in.dual_mode_valid=1;
    in.window_valid=1;in.current_scale_qualified=1;in.timing_qualified=1;
    in.adc1_raw=2049;in.adc2_raw=2049;
    in.jsqr1=stock_current_jsqr_one(4);in.jsqr2=stock_current_jsqr_one(5);
}
static void latch(uint32_t epoch){
    motor_id_adc_feedback_queue(&f,2040,1980,1980,0,1,epoch);
    motor_id_adc_feedback_on_update(&f,in.now_cycles-1000);
}
int main(void){
    setup();
    CHECK(!motor_id_adc_feedback_capture(&f,&in,&out)); /* no physical UEV */
    motor_id_adc_feedback_queue(&f,2040,1980,1980,0,1,7);
    CHECK(!motor_id_adc_feedback_capture(&f,&in,&out)); /* queued != applied */
    motor_id_adc_feedback_on_update(&f,in.now_cycles-1000);
    CHECK(motor_id_adc_feedback_capture(&f,&in,&out));
    CHECK(out.adc_valid && out.voltage_valid && out.command_epoch==7);
    CHECK(out.applied_voltage_mv>=350 && out.applied_voltage_mv<=370);
    CHECK(out.phase_current_ma>=95 && out.phase_current_ma<=110);
    CHECK(!out.bemf_valid && !out.speed_valid); /* point 2.3 still separate */
    CHECK(f.samples_accepted==1);
    in.jsqr1=stock_current_jsqr_one(3);
    CHECK(!motor_id_adc_feedback_capture(&f,&in,&out));
    CHECK(!out.adc_valid && !out.voltage_valid);
    in.jsqr1=stock_current_jsqr_one(4);
    in.adc2_ch5_offset_valid=0;
    CHECK(!motor_id_adc_feedback_capture(&f,&in,&out));
    in.adc2_ch5_offset_valid=1;
    in.adc2_jeoc=0;
    CHECK(!motor_id_adc_feedback_capture(&f,&in,&out));
    in.adc2_jeoc=1;
    in.current_scale_qualified=0;
    CHECK(!motor_id_adc_feedback_capture(&f,&in,&out));
    in.current_scale_qualified=1;
    in.timing_qualified=0;
    CHECK(!motor_id_adc_feedback_capture(&f,&in,&out));
    in.timing_qualified=1;
    in.now_cycles+=20000; /* >250us after last TIM1 update */
    CHECK(!motor_id_adc_feedback_capture(&f,&in,&out));
    motor_id_adc_feedback_on_update(&f,in.now_cycles-1000);
    CHECK(motor_id_adc_feedback_capture(&f,&in,&out));
    in.vbus_age_ms=101;
    CHECK(!motor_id_adc_feedback_capture(&f,&in,&out));
    in.vbus_age_ms=0;
    in.vbus_mv=0;
    CHECK(!motor_id_adc_feedback_capture(&f,&in,&out));
    in.vbus_mv=36000;
    in.adc2_raw=4096;
    CHECK(!motor_id_adc_feedback_capture(&f,&in,&out));
    in.adc2_raw=2049;
    motor_id_adc_feedback_off(&f);
    CHECK(!motor_id_adc_feedback_capture(&f,&in,&out));
    CHECK(!f.active_valid && !f.pending);
    /* Full 4-quadrant projection, reconstructed applied voltage signed. */
    for(int n=0;n<4;n++){
        setup();
        const uint16_t angle=(uint16_t)(n*16384);
        const uint8_t sector=(n==0)?1:(n==1)?2:(n==2)?4:5;
        const stock_adc_pair_t p=stock_current_pair_for_sector(sector);
        in.jsqr1=stock_current_jsqr_one(p.adc1_channel);
        in.jsqr2=stock_current_jsqr_one(p.adc2_channel);
        motor_id_adc_feedback_queue(&f,2040,1980,1980,angle,sector,100+n);
        motor_id_adc_feedback_on_update(&f,in.now_cycles-1000);
        CHECK(motor_id_adc_feedback_capture(&f,&in,&out));
        CHECK(out.command_epoch==(uint32_t)(100+n));
        CHECK(out.applied_voltage_mv<=400 && out.applied_voltage_mv>=-400);
    }
    setup();latch(3);
    in.adc2_ch5_offset=2028;in.adc2_raw=2029; /* channel offset independent */
    CHECK(motor_id_adc_feedback_capture(&f,&in,&out));
    CHECK(out.phase_current_ma>=95 && out.phase_current_ma<=110);
    puts("PASS: v0.9.11 ADC feedback: update shadow promotion, 6 validation classes, four angles, offsets, epoch, sample time, gate-off");
    return 0;
}
