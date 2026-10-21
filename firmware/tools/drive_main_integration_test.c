/* Compile the real main.c decisions with inert CPU barriers and allocated
 * register memory. No motor/ADC/flash model and no device connection. */
#define _GNU_SOURCE
#include <assert.h>
#include <stdio.h>
#include <sys/mman.h>
#include <string.h>
#define main target_main
#include "main.c"
#undef main
uint32_t _sidata,_sdata,_edata,_sbss,_ebss,_estack;

static void registers(void) {
    assert(mmap((void *)0x40000000,0x30000,PROT_READ|PROT_WRITE,
                MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED,-1,0)!=(void *)-1);
    assert(mmap((void *)0xE0000000,0x100000,PROT_READ|PROT_WRITE,
                MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED,-1,0)!=(void *)-1);
}
static void fresh_roll(void) {
    static const uint16_t abc[6][3]={{3000,2000,1000},{2000,3000,1000},
        {1000,3000,2000},{1000,2000,3000},{2000,1000,3000},{3000,1000,2000}};
    uint32_t now=1000000;
    g30_rolling_bemf_reset(&g_rolling_bemf);
    for(unsigned k=0;k<7;k++) {
        unsigned ix=(6-k%6)%6;
        g30_rolling_bemf_sample(&g_rolling_bemf,abc[ix][0],abc[ix][1],abc[ix][2],now);
        now+=400000;
    }
    DWT_CYCCNT=now-400000+1000;
    assert(g30_rolling_bemf_ready(&g_rolling_bemf,DWT_CYCCNT));
}
static void dash(uint8_t throttle,uint8_t brake) {
    ninebot_frame_t f={0};
    f.format=NINEBOT_FMT_5AA5;f.src=G30_DASH_ADDR;f.dst=G30_ESC_ADDR;
    f.cmd=G30_DASH_CMD_ADC;f.payload_len=3;f.payload[1]=throttle;f.payload[2]=brake;
    process_frame(&f);
}
static void setup(void) {
    memset((void *)0x40000000,0,0x30000);
    g_gate_kill_latched=0;g_safety_latched=0;g_power_armed=0;
    g_motor_id_vector_owner=0;g_id_bench_requested=0;g_poweroff_requested=0;
    g_offset_cal_active=0;g_offset_cal_valid=1;
    g_adc2_ch4_offset_valid=1;g_adc2_ch5_offset_valid=1;
    g_ms=4000;g_adc_last_ms=4000;g_vbus_last_ms=4000;g_last_abs_current=0;
    g_adc_raw[3]=2500;g_vbus_pending=0;
    for(unsigned i=0;i<3;i++)g_adc_offset[i]=2048;
    sensorless_control_init(&ctrl);
    /* Test fixture numbers only; never declared measured G30 parameters. */
    assert(sensorless_control_set_motor_params(&ctrl,200000,300000,15000));
    assert(sensorless_control_set_run_current_ma(&ctrl,600));
    motor_config_txn_init(&g_motor_cfg,600);
    const motor_pipeline_io_t io={motor_hw_arm,motor_hw_gate_off,
        motor_hw_select_adc,motor_hw_pwm,motor_hw_coast};
    motor_pipeline_init(&g_motor_pipeline,&ctrl,io,0,PWM_ARR,HARD_OC_COUNTS);
    const g30_passive_bemf_adc_io_t passive={passive_bemf_regular_start,
        passive_bemf_regular_poll,passive_bemf_regular_abort};
    g30_passive_bemf_adc_init(&g_passive_bemf_adc,passive,0);
    g30_rolling_bemf_config_t cfg;g30_rolling_bemf_default_config(&cfg);
    g30_rolling_bemf_init(&g_rolling_bemf,&cfg);
    gate_pins_to_safe_inputs();
    TIM_BDTR(TIM1_BASE)=DEADTIME_DTG|D_GUARD_OSSI|D_GUARD_OSSR|D_GUARD_MOE;
    TIM_CCER(TIM1_BASE)=STOCK_TIM1_GATE_ADC_CC4E;
    iap_update_init();
    dash(0,0);fresh_roll();dash(255,0);
}
static void iap_begin_pending(void) {
    /* Native parser from unchanged iap_update.c; DO NOT poll or program.
     * Complete valid BEGIN: 256 bytes, same stock wire format as before. */
    uint8_t frame[]={0x5a,0xa5,4,0x3e,0x20,7,0,0,1,0,0,0,0};
    uint16_t sum=0;
    for(unsigned i=2;i<sizeof(frame)-2;i++)sum=(uint16_t)(sum+frame[i]);
    sum=(uint16_t)~sum;frame[sizeof(frame)-2]=(uint8_t)sum;frame[sizeof(frame)-1]=(uint8_t)(sum>>8);
    for(unsigned i=0;i<sizeof(frame);i++)iap_update_feed(frame[i]);
    assert(iap_update_pending());
}

int main(void) {
    registers();
    setup();
    assert(!drive_start_blockers(commissioning_guard_mask(),1));
    assert(motor_request_rolling());
    assert(g_power_armed && g_motor_pipeline.powered && ctrl.drive_request);
    assert(gate_safety_check()); /* previously rejected every rolling RC */
    int full=ctrl.run_iq_counts;
    dash(128,0);assert(ctrl.run_iq_counts>0 && ctrl.run_iq_counts<full);
    dash(0,0);
    assert(!g_power_armed && !g_motor_pipeline.powered && !ctrl.drive_request);
    assert(!g_gate_kill_latched && !g_safety_latched);
    assert(stock_tim1_gate_ccer_is_off((uint16_t)TIM_CCER(TIM1_BASE)));
    dash(0,0);fresh_roll();dash(255,0);drive_runtime_poll();
    assert(g_power_armed && gate_safety_check());
    dash(255,80);
    assert(!g_power_armed && !g_gate_kill_latched && ctrl.run_iq_counts==0);
    drive_runtime_poll();assert(!g_power_armed);
    /* Releasing brake while still holding the old gas request cannot
     * restart. Fresh neutral -> rolling motion -> new gas must work. */
    dash(255,0);fresh_roll();drive_runtime_poll();
    assert(!g_power_armed && !g_motor_pipeline.neutral_seen);
    dash(0,0);assert(g_motor_pipeline.neutral_seen);
    fresh_roll();dash(180,0);drive_runtime_poll();
    assert(g_power_armed && !g_gate_kill_latched && gate_safety_check());
    dash(0,0);
    assert(!g_power_armed && !g_gate_kill_latched);
    puts("PASS actual main.c: proportional gas, brake-coast, neutral and real rolling rearm");

    /* Braking while ALREADY coasting must invalidate any previous neutral.
     * Otherwise an old neutral can authorize gas at brake release. */
    setup();
    dash(0,0);assert(g_motor_pipeline.neutral_seen);
    dash(0,60);assert(!g_motor_pipeline.neutral_seen);
    dash(255,60);dash(255,0);fresh_roll();drive_runtime_poll();
    assert(!g_power_armed && !g_gate_kill_latched);
    dash(0,0);fresh_roll();dash(180,0);drive_runtime_poll();
    assert(g_power_armed && !g_gate_kill_latched);
    puts("PASS actual main.c: braking during coast requires fresh neutral");

    /* A 0x64 status heartbeat must not refresh 0x65 throttle freshness. */
    setup();
    const uint32_t last_control_ms=g_dash_last_ms;
    g_ms+=COMM_DASH_STALE_MS+1u;
    USART_SR_REG(USART2_BASE)=USART_SR_TXE|USART_SR_TC;
    ninebot_frame_t heartbeat={0};
    heartbeat.format=NINEBOT_FMT_5AA5;
    heartbeat.src=G30_DASH_ADDR;heartbeat.dst=G30_ESC_ADDR;
    heartbeat.cmd=G30_DASH_CMD_STATUS;
    process_frame(&heartbeat);
    assert(g_dash_last_ms==last_control_ms);
    drive_runtime_poll();
    assert(!g_power_armed && !g_gate_kill_latched);
    /* A fresh ADC report really does renew control freshness. */
    dash(0,0);fresh_roll();dash(180,0);drive_runtime_poll();
    assert(g_power_armed && !g_gate_kill_latched);
    puts("PASS actual main.c: status heartbeat cannot substitute fresh 0x65 input");

    setup();iap_begin_pending();
    assert(!motor_request_rolling()); /* update ownership always wins */
    assert(!g_power_armed && stock_tim1_gate_ccer_is_off((uint16_t)TIM_CCER(TIM1_BASE)));

    for(unsigned kind=0;kind<4;kind++) {
        setup();assert(motor_request_rolling());
        if(kind==0) {
            ninebot_frame_t e6={0};e6.arg=0xe6;e6.cmd=NINEBOT_WRITE_NR;
            app_write_action(&e6);
        } else if(kind==1)motor_pipeline_stop(&g_motor_pipeline,MOTOR_STOP_ADC);
        else if(kind==2)motor_pipeline_watchdog(&g_motor_pipeline,4251,250);
        else {
            TIM_BDTR(TIM1_BASE)^=1u;
            assert(!gate_safety_check());
        }
        assert(g_gate_kill_latched && !g_power_armed);
        iap_begin_pending();assert(!motor_request_rolling());
    }
    puts("PASS actual unchanged IAP parser: pending BEGIN blocks start; E6, ADC, lost dashboard and gate fault retain parser access");
    setup();
    USART_SR_REG(USART2_BASE)=USART_SR_TXE|USART_SR_TC;
    uint8_t bad_order[]={0x5a,0xa5,0,0x3e,0x20,8,0,0,0};
    uint16_t sum=0;
    for(unsigned i=2;i<sizeof(bad_order)-2;i++)sum=(uint16_t)(sum+bad_order[i]);
    sum=(uint16_t)~sum;bad_order[7]=(uint8_t)sum;bad_order[8]=(uint8_t)(sum>>8);
    for(unsigned i=0;i<sizeof(bad_order);i++)iap_update_feed(bad_order[i]);
    iap_update_poll(); /* Order rejection and ACK only, no flash access. */
    assert(iap_update_state()==IAP_ERROR && iap_update_last_error()==4);
    iap_begin_pending();
    puts("PASS actual IAP poll: protocol error does not veto a fresh BEGIN");
    setup();dash(0,0);
    ninebot_frame_t cfg={0};cfg.cmd=NINEBOT_WRITE_NR;
    cfg.payload[0]=0xde;cfg.payload[1]=0xc0;cfg.payload_len=6;
    const uint32_t tuple[3]={280000u,650000u,16000u};
    for(unsigned k=0;k<3;k++) {
        cfg.arg=(uint8_t)(0xf0+k);put_u32(cfg.payload+2,tuple[k]);
        app_write_config(&cfg);assert(g_last_action_status==ACT_RAM_ONLY);
    }
    cfg.arg=0xf3;cfg.payload_len=4;put_i16(cfg.payload+2,1234);
    app_write_config(&cfg);assert(g_last_action_status==ACT_RAM_ONLY);
    cfg.arg=0xf4;put_u16(cfg.payload+2,500);
    app_write_config(&cfg);assert(g_last_action_status==ACT_RAM_ONLY);
    cfg.arg=0xf5;cfg.payload_len=2;
    app_write_config(&cfg);assert(g_last_action_status==ACT_RAM_ONLY);
    assert(ctrl.motor_r_uohm==tuple[0] && ctrl.motor_l_nh==tuple[1]);
    assert(ctrl.phase_offset_u16==1234 && g_rolling_bemf.cfg.phase_offset_u16==1234);
    cfg.arg=0xff;cfg.payload_len=5;cfg.payload[2]=6;put_u16(cfg.payload+3,800);
    app_write_config(&cfg);assert(g_last_action_status==ACT_RAM_ONLY);
    assert(g_rolling_bemf.cfg.pole_pairs==6 && g_rolling_bemf.cfg.wheel_circ_mm==800);
    cfg.payload[2]=0;app_write_config(&cfg);assert(g_last_action_status==ACT_BAD_RANGE);
    assert(g_rolling_bemf.cfg.pole_pairs==6);
    sensorless_control_init(&ctrl);motor_config_txn_init(&g_motor_cfg,500);
    motor_profile_from_build();passive_bemf_init();
#if G30_PROFILE_R_UOHM
    assert(ctrl.motor_params_valid && ctrl.motor_r_uohm==G30_PROFILE_R_UOHM);
    assert(g_motor_cfg.active_valid && g_cfg_phase_offset==G30_PROFILE_PHASE_OFFSET_U16);
    assert(g_rolling_bemf.cfg.pole_pairs==G30_PROFILE_POLE_PAIRS);
    assert(g_rolling_bemf.cfg.wheel_circ_mm==G30_PROFILE_CIRCUMFERENCE_MM);
    assert(g_rolling_bemf.cfg.phase_offset_u16==G30_PROFILE_PHASE_OFFSET_U16);
    puts("PASS built-in startup profile with explicit HOST-ONLY fixture; not measured G30 data");
#else
    assert(!ctrl.motor_params_valid);
    assert(g_rolling_bemf.cfg.pole_pairs==0 && g_rolling_bemf.cfg.wheel_circ_mm==0);
    puts("PASS startup keeps unknown motor profile/geometry unset");
#endif
    puts("PASS actual config path: transactional R/L/flux, shared phase offset and validated RAM geometry");
    puts("No flash erase/program/reset or physical recovery was tested.");
    return 0;
}
