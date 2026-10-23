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
    g_debug_uart_muted=0u;
    g_motor_id_vector_owner=0;g_id_bench_requested=0;g_poweroff_requested=0;
    g_offset_cal_active=0;g_offset_cal_valid=1;
    g_drive_cal_quiet=0u;g_drive_cal_owned=0u;
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
    g30_bms_link_init(g_ms);
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

static void bms_soc_bytes(uint8_t soc) {
    uint8_t f[]={0x5a,0xa5,2,0x22,0x20,4,0x32,soc,0,0,0};
    unsigned sum=0;
    for(unsigned i=2;i<sizeof f-2;i++)sum+=f[i];
    f[9]=(uint8_t)~sum;f[10]=(uint8_t)((~sum)>>8);
    for(unsigned i=0;i<sizeof f;i++) {
        USART_SR_REG(G30_BMS_USART3_BASE)=USART_SR_RXNE;
        USART_DR_REG(G30_BMS_USART3_BASE)=f[i];
        USART3_IRQHandler();
    }
    USART_SR_REG(G30_BMS_USART3_BASE)=0u;
}
static void bms_soc_reply(uint8_t soc) {
    bms_soc_bytes(soc);
    g30_bms_link_poll(g_ms,0u);
}

static void test_battery_and_iap_isolation(void) {
    setup();
    REG32(0x40010004u)=0x80u|0x30u|0x08u;
    USART_CR1_REG(USART2_BASE)=0x2044u;
    uart1_debug_init();
    assert(REG32(0x40010004u)==(0x80u|0x30u|0x08u|0x04u));
    g30_bms_link_init(g_ms);
    assert(REG32(0x40010004u)==(0x80u|0x08u|0x04u));
    assert(USART_CR1_REG(USART2_BASE)==0x2044u);
    uint8_t p[16],status[6],tx[10];
    const uint32_t dashboard_at=g_dash_last_ms;
    const uint32_t uart2=USART_CR1_REG(USART2_BASE);
    const uint32_t gate=TIM_CCER(TIM1_BASE);
    stock_dashboard_status(status);assert(status[1]==0u);
    g30_bms_link_poll(g_ms,0u);
    for(unsigned i=0;i<sizeof tx;i++) {
        USART_SR_REG(G30_BMS_USART3_BASE)=USART_SR_TXE;
        USART3_IRQHandler();tx[i]=(uint8_t)USART_DR_REG(G30_BMS_USART3_BASE);
    }
    const uint8_t read[]={0x5a,0xa5,1,0x20,0x22,1,0x32,2,0x87,0xff};
    assert(!memcmp(tx,read,sizeof read));
    assert(!(USART_CR1_REG(G30_BMS_USART3_BASE)&(1u<<7)));
    bms_soc_reply(73u);stock_dashboard_status(status);
    assert(status[1]==73u && g_dash_last_ms==dashboard_at);
    assert(USART_CR1_REG(USART2_BASE)==uart2 && TIM_CCER(TIM1_BASE)==gate);
    assert(!g_power_armed && !g_gate_kill_latched && !g_safety_latched);
    /* SOC frames never count as drive commands or renew a gas heartbeat. */
    g_ms+=G30_BMS_SOC_TIMEOUT_MS+1u;
    stock_dashboard_status(status);assert(status[1]==0u);
    g30_bms_link_status(g_ms,p);assert(p[4]==2u);

    /* A delayed main loop uses byte arrival time, not its later drain time. */
    bms_soc_bytes(99u);g_ms+=G30_BMS_SOC_TIMEOUT_MS+1u;
    g30_bms_link_poll(g_ms,0u);stock_dashboard_status(status);
    assert(status[1]==0u);

    /* A missing UART TX interrupt times out, rather than blocking IAP. */
    g30_bms_link_poll(g_ms,0u);g_ms+=21u;g30_bms_link_poll(g_ms,0u);
    assert(!(USART_CR1_REG(G30_BMS_USART3_BASE)&(1u<<7)));
    g30_bms_link_status(g_ms,p);assert(p[14]!=0u);
    iap_begin_pending();
    assert(!motor_request_rolling());
    g30_bms_link_poll(g_ms,1u);
    assert(iap_update_pending());
    assert(!(USART_CR1_REG(G30_BMS_USART3_BASE)&(1u<<7)));
    /* No queued background read is resumed during updater ownership. */
    g_ms+=10000u;g30_bms_link_poll(g_ms,1u);
    assert(iap_update_pending() && !(USART_CR1_REG(G30_BMS_USART3_BASE)&(1u<<7)));

    setup();
    /* UART framing/overrun and a saturated RX ring do not affect IAP or
     * power state; the next clean battery packet is accepted again. */
    for(unsigned i=0;i<80u;i++) {
        USART_SR_REG(G30_BMS_USART3_BASE)=USART_SR_RXNE;
        USART_DR_REG(G30_BMS_USART3_BASE)=0x5au;USART3_IRQHandler();
    }
    g30_bms_link_poll(g_ms,0u);
    USART_SR_REG(G30_BMS_USART3_BASE)=USART_SR_ORE;USART3_IRQHandler();
    g30_bms_link_poll(g_ms,0u);
    bms_soc_reply(31);g30_bms_link_status(g_ms,p);
    assert(p[4]==3u && p[5]==31u && p[14]!=0u);
    assert(!g_power_armed && !g_gate_kill_latched && !g_safety_latched);
    iap_begin_pending();assert(iap_update_pending());
    puts("PASS actual main.c: BMS SOC -> dashboard, nonblocking UART, overflow recovery and unchanged IAP ownership");
}

static void quiet_startup(void) {
    g_adc_samples=ARM_MIN_ADC_SAMPLES;
    g_adc_last_ms=g_ms;
    g30_rolling_bemf_sample(&g_rolling_bemf,2048,2048,2048,DWT_CYCCNT);
    dash(0,0);
}
static void test_peripheral_failure_returns(void) {
    setup();
    USART_SR=0u;USART_DR=0x99u;
    uart_putc('X');assert(g_debug_uart_muted && USART_DR==0x99u);
    USART_SR=USART_SR_TXE;uart_putc('Y');assert(USART_DR==0x99u);
    iap_begin_pending();assert(iap_update_pending());

    setup();
    volatile uint32_t flags=ADC_CR2_RSTCAL;
    assert(!adc_wait_clear(&flags,ADC_CR2_RSTCAL));
    flags=ADC_CR2_CAL;assert(!adc_wait_clear(&flags,ADC_CR2_CAL));
    flags=0;assert(adc_wait_clear(&flags,ADC_CR2_CAL));
    /* Allocated peripheral memory never autonomously clears RSTCAL.
     * The actual init must return with gates off, so boot can reach IAP. */
    adc1_injected_init();
    assert(g_safety_latched==0xA014u && g_gate_kill_latched && !g_power_armed);
    iap_begin_pending();assert(iap_update_pending());
    puts("PASS actual main.c: stuck debug UART and ADC calibration return without blocking unchanged IAP parser");
}
static void test_automatic_offsets(void) {
    setup();g_offset_cal_valid=0u;
    const uint16_t zero[3]={2048,2048,2048};
    g30_fixed_scan_init(&g_fixed_scan,zero);
    quiet_startup();drive_startup_calibration_poll();
    assert(!g_offset_cal_active && g_drive_cal_quiet && !g_power_armed);
    g_ms+=99;quiet_startup();drive_startup_calibration_poll();
    assert(!g_offset_cal_active);
    g_ms++;quiet_startup();drive_startup_calibration_poll();
    assert(g_offset_cal_active && g_drive_cal_owned && !g_power_armed);
    assert(stock_tim1_gate_ccer_is_off((uint16_t)TIM_CCER(TIM1_BASE)));
    /* Actual 512-sample calibration with explicit host ADC fixtures,
     * then the same publishing helper used by the real ADC IRQ. */
    for(unsigned i=0;i<G30_FIXED_OFFSET_SAMPLES;i++)
        assert(!g30_fixed_scan_capture(&g_fixed_scan,G30_FIXED_JSQR_A,G30_FIXED_JSQR_B,
            STOCK_ADC_MODE_REG_INJEC_SIMULT|ADC_CR1_SCAN,ADC_CR1_SCAN,
            ADC_SR_JEOC,ADC_SR_JEOC,2048,2048,2048,2048));
    finish_offset_calibration();drive_startup_calibration_poll();
    assert(g_offset_cal_valid && !g_offset_cal_active && !g_drive_cal_owned);
    fresh_roll();dash(180,0);drive_runtime_poll();
    assert(g_power_armed && !g_gate_kill_latched);

    setup();g_offset_cal_valid=0;g30_fixed_scan_init(&g_fixed_scan,zero);
    quiet_startup();drive_startup_calibration_poll();
    g_ms+=100;quiet_startup();dash(90,0);drive_startup_calibration_poll();
    assert(!g_offset_cal_active && !g_drive_cal_quiet);
    quiet_startup();drive_startup_calibration_poll();
    g_ms+=100;quiet_startup();drive_startup_calibration_poll();
    assert(g_offset_cal_active);
    /* Movement during calibration discards partial offsets, never applies
     * them as motor qualification, and never produces a motor command. */
    fresh_roll();drive_startup_calibration_poll();
    assert(!g_offset_cal_active && !g_offset_cal_valid && !g_fixed_scan.offset_valid);
    assert(!g_power_armed && !g_gate_kill_latched);
    quiet_startup();drive_startup_calibration_poll();
    g_ms+=100;quiet_startup();drive_startup_calibration_poll();
    assert(g_offset_cal_active);
    iap_begin_pending();drive_startup_calibration_poll();
    assert(iap_update_pending() && !g_offset_cal_active && !g_power_armed);

    setup();g_offset_cal_valid=0;g30_fixed_scan_init(&g_fixed_scan,zero);
    quiet_startup();g_safety_latched=0xA004;
    drive_startup_calibration_poll();assert(!g_offset_cal_active && g_safety_latched==0xA004);
    iap_begin_pending();assert(iap_update_pending());
    puts("PASS actual main.c: automatic stationary startup offsets, real 512-sample helper, standalone rollstart and IAP/fault precedence");
}

int main(void) {
    registers();
    test_battery_and_iap_isolation();
    test_peripheral_failure_returns();
    test_automatic_offsets();
    setup();
    assert(!drive_start_blockers(commissioning_guard_mask(),1));
    assert(motor_request_rolling());
    assert(g_power_armed && g_motor_pipeline.powered && ctrl.drive_request);
    assert(gate_safety_check()); /* previously rejected every rolling RC */
    int full=ctrl.run_iq_counts;
    dash(128,0);assert(ctrl.run_iq_counts>0 && ctrl.run_iq_counts<full);
    dash(0,0);
    assert(!g_power_armed && !g_motor_pipeline.powered && !ctrl.drive_request);
    assert(g_motor_pipeline.neutral_seen); /* this single real release counts */
    assert(!g_gate_kill_latched && !g_safety_latched);
    assert(stock_tim1_gate_ccer_is_off((uint16_t)TIM_CCER(TIM1_BASE)));
    fresh_roll();dash(255,0);drive_runtime_poll();
    assert(g_power_armed && gate_safety_check());
    dash(255,80);
    assert(!g_power_armed && !g_gate_kill_latched && ctrl.run_iq_counts==0);
    assert(!ctrl.drive_request && !g_motor_pipeline.powered);
    assert(stock_tim1_gate_ccer_is_off((uint16_t)TIM_CCER(TIM1_BASE)));
    assert(tim1_bdtr_guard_gpio_down((uint16_t)gate_pin_mode_word_a(),
        (uint16_t)gate_pin_mode_word_b(),(uint16_t)GPIO_ODR(GPIOA_BASE),
        (uint16_t)GPIO_ODR(GPIOB_BASE)));
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
    /* Refresh simulated ADC/VBUS timestamps too: this host fixture has
     * no live ADC IRQ, unlike the real target hardware. */
    g_adc_last_ms=g_ms;g_vbus_last_ms=g_ms;
    /* A fresh ADC report really does renew control freshness. */
    dash(0,0);fresh_roll();dash(180,0);drive_runtime_poll();
    assert(g_power_armed && !g_gate_kill_latched);
    puts("PASS actual main.c: status heartbeat cannot substitute fresh 0x65 input");

    /* A coasting controller cannot retain a pre-timeout neutral and then
     * accept fresh GAS without a fresh neutral command. */
    setup();
    dash(0,0);
    assert(g_motor_pipeline.neutral_seen);
    g_ms+=COMM_DASH_STALE_MS+1u;
    motor_pipeline_watchdog(&g_motor_pipeline,g_ms,COMM_DASH_STALE_MS);
    assert(!g_motor_pipeline.neutral_seen && !g_power_armed);
    assert(!g_gate_kill_latched);
    g_adc_last_ms=g_ms;g_vbus_last_ms=g_ms;
    fresh_roll();dash(180,0);drive_runtime_poll();
    assert(!g_power_armed && !g_gate_kill_latched);
    dash(0,0);fresh_roll();dash(180,0);drive_runtime_poll();
    assert(g_power_armed && gate_safety_check());
    puts("PASS actual main.c: expired coast neutral needs fresh throttle-zero");

    /* While armed, a stale input is an irreversible MOTOR_STOP_DASH fault,
     * even when status packets keep arriving. No automatic resume. */
    setup();
    assert(motor_request_rolling());
    g_ms+=COMM_DASH_STALE_MS+1u;
    motor_pipeline_watchdog(&g_motor_pipeline,g_ms,COMM_DASH_STALE_MS);
    assert(g_gate_kill_latched && !g_power_armed);
    g_adc_last_ms=g_ms;g_vbus_last_ms=g_ms;
    dash(0,0);fresh_roll();dash(180,0);drive_runtime_poll();
    assert(!g_power_armed && g_gate_kill_latched);
    puts("PASS actual main.c: powered dashboard timeout remains terminal");

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
