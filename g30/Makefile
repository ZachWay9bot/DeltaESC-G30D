CC ?= clang
OBJCOPY ?= llvm-objcopy
OBJDUMP ?= llvm-objdump
CFLAGS_COMMON = -g3 --target=arm-none-eabi -mcpu=cortex-m3 -mthumb -ffreestanding -fno-builtin -fdata-sections -ffunction-sections -Oz -Wall -Wextra -Werror -Isrc
LDFLAGS_COMMON = --target=arm-none-eabi -mcpu=cortex-m3 -mthumb -nostdlib -Wl,-T,linker.ld,--gc-sections
SRC = src/main.c src/sensorless_control.c src/vesc_ebics_flux.c src/ninebot_link.c src/iap_update.c src/iap_proto.c src/iap_control.c src/dashboard_runtime.c src/power_logic.c src/motor_probe.c src/ble_motor_probe.c src/commissioning_guard.c src/stock_current_frontend.c src/g30_adc_current_window.c src/g30_adc_current_offsets.c src/stock_dual_adc_plan.c src/stock_dual_adc_runtime.c src/stock_adc_quality.c src/stock_adc_snapshot.c src/stock_adc_timing.c src/stock_adc_correlated.c src/stock_adc_mode_guard.c src/g30_fixed_scan.c src/motor_config_txn.c src/foc_adc_contract.c src/foc_adc_motor_handoff.c src/motor_id.c src/motor_id_auto.c src/motor_id_vector_output.c src/motor_id_adc_feedback.c src/motor_id_flux_feedback.c src/motor_pipeline.c src/stock_tim1_gate.c src/aeabi_mem.c src/g30_rolling_bemf.c src/g30_passive_bemf_adc.c
SAFE_ELF = build/DeltaESC_G30D_v0_9_20_ble_syncsafe.elf
SAFE_BIN = build/DeltaESC_G30D_v0_9_20_ble_syncsafe.bin
ACTIVE_ELF = build/DeltaESC_G30D_v0_9_20_ble_observer_zero_vector.elf
ACTIVE_BIN = build/DeltaESC_G30D_v0_9_20_ble_observer_zero_vector.bin
BENCH_ELF = build/DeltaESC_G30D_v0_9_20_ble_sensorless_bench.elf
BENCH_BIN = build/DeltaESC_G30D_v0_9_20_ble_sensorless_bench.bin
all: safe
safe: $(SAFE_BIN)
build: ; mkdir -p build
$(SAFE_ELF): $(SRC) linker.ld | build
	$(CC) $(CFLAGS_COMMON) -DPOWER_STAGE_ARM_ALLOWED=0 -DSENSORLESS_RUN_ALLOWED=0 -DSENSORLESS_CLOSED_LOOP_ALLOWED=1 $(SRC) $(LDFLAGS_COMMON) -Wl,--undefined=motor_id_accept -Wl,--undefined=motor_id_commit -Wl,--undefined=motor_id_vector_output_request -Wl,--undefined=motor_id_adc_feedback_capture -Wl,--undefined=motor_id_auto_tick -Wl,--undefined=motor_id_auto_bind_config -Wl,--undefined=g30_adc_current_flags -Wl,--undefined=g30_adc2_current_offset_normalize -Wl,-Map=build/v0_9_20_syncsafe.map -o $@
$(SAFE_BIN): $(SAFE_ELF)
	$(OBJCOPY) -O binary $< $@
	$(OBJDUMP) -h $< > build/v0_9_20_syncsafe_sections.txt
clean: ; rm -rf build

# Explicit motor-ID-only experiment: normal drive flags stay zero.
ID_BENCH_ELF = build/DeltaESC_G30D_v0_9_20_motor_id_bench.elf
ID_BENCH_BIN = build/DeltaESC_G30D_v0_9_20_motor_id_bench.bin
id-bench: $(ID_BENCH_BIN)
$(ID_BENCH_ELF): $(SRC) linker.ld | build
	$(CC) $(CFLAGS_COMMON) -DPOWER_STAGE_ARM_ALLOWED=0 -DSENSORLESS_RUN_ALLOWED=0 -DMOTOR_ID_BENCH_ALLOWED=1 $(SRC) $(LDFLAGS_COMMON) -Wl,-Map=build/v0_9_20_motor_id_bench.map -o $@
$(ID_BENCH_BIN): $(ID_BENCH_ELF)
	$(OBJCOPY) -O binary $< $@
	$(OBJDUMP) -h $< > build/v0_9_20_motor_id_bench_sections.txt


# v0.9.20 source/link proof only. There is intentionally no .bin target:
# this target must not become an easy-to-flash motor image.
SENSORLESS_LOCKED_ELF = build/DeltaESC_G30D_v0_9_20_sensorless_locked.elf
sensorless-locked: $(SENSORLESS_LOCKED_ELF)
$(SENSORLESS_LOCKED_ELF): $(SRC) linker.ld | build
	$(CC) $(CFLAGS_COMMON) -DPOWER_STAGE_ARM_ALLOWED=0 -DSENSORLESS_RUN_ALLOWED=1 -DSENSORLESS_CLOSED_LOOP_ALLOWED=1 -DMOTOR_ID_BENCH_ALLOWED=0 $(SRC) $(LDFLAGS_COMMON) -Wl,--undefined=sensorless_control_step -Wl,--undefined=vesc_ebics_flux_step -Wl,-Map=build/v0_9_20_sensorless_locked.map -o $@
	$(OBJDUMP) -h $@ > build/v0_9_20_sensorless_locked_sections.txt


# v0.10.0 FO69/FO70 source/link proof only. There is intentionally no BIN.
XBOT_ROLL_LOCKED_ELF = build/DeltaESC_G30D_v0_10_0_xbot_rolling_locked.elf
xbot-rolling-locked: $(XBOT_ROLL_LOCKED_ELF)
$(XBOT_ROLL_LOCKED_ELF): $(SRC) linker.ld | build
	$(CC) $(CFLAGS_COMMON) -DPOWER_STAGE_ARM_ALLOWED=0 -DSENSORLESS_RUN_ALLOWED=1 -DSENSORLESS_CLOSED_LOOP_ALLOWED=1 -DMOTOR_ID_BENCH_ALLOWED=0 -DPASSIVE_BEMF_PROBE_ALLOWED=1 $(SRC) $(LDFLAGS_COMMON) -Wl,--undefined=g30_rolling_bemf_sample -Wl,--undefined=g30_passive_bemf_adc_tick -Wl,--undefined=sensorless_control_start_rolling -Wl,--undefined=motor_pipeline_start_rolling -Wl,-Map=build/v0_10_0_xbot_rolling_locked.map -o $@
	$(OBJDUMP) -h $@ > build/v0_10_0_xbot_rolling_locked_sections.txt


# v0.10.1 first active rolling sensorless RC.
# Produces a BIN intentionally; this is bench/current-limited bring-up, not ride release.
ROLLING_ACTIVE_RC_ELF = build/DeltaESC_G30D_v0_10_1_rolling_active_rc.elf
ROLLING_ACTIVE_RC_BIN = build/DeltaESC_G30D_v0_10_1_rolling_active_rc.bin
rolling-active-rc: $(ROLLING_ACTIVE_RC_BIN)
$(ROLLING_ACTIVE_RC_ELF): $(SRC) linker.ld | build
	$(CC) $(CFLAGS_COMMON) -DPOWER_STAGE_ARM_ALLOWED=1 -DSENSORLESS_RUN_ALLOWED=1 -DSENSORLESS_CLOSED_LOOP_ALLOWED=1 -DMOTOR_ID_BENCH_ALLOWED=0 -DPASSIVE_BEMF_PROBE_ALLOWED=1 -DROLLING_SENSORLESS_RC_ALLOWED=1 $(SRC) $(LDFLAGS_COMMON) -Wl,--undefined=g30_rolling_bemf_sample -Wl,--undefined=g30_passive_bemf_adc_tick -Wl,--undefined=sensorless_control_start_rolling -Wl,--undefined=motor_pipeline_start_rolling -Wl,-Map=build/v0_10_1_rolling_active_rc.map -o $@
	$(OBJDUMP) -h $@ > build/v0_10_1_rolling_active_rc_sections.txt
$(ROLLING_ACTIVE_RC_BIN): $(ROLLING_ACTIVE_RC_ELF)
	$(OBJCOPY) -O binary $< $@
