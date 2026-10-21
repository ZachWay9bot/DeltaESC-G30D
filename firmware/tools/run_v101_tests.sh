#!/usr/bin/env bash
# Software-only checks. No external hardware, no flashing, no motor enable.
set -euo pipefail
cd "$(dirname "$0")/.."
python3 tools/pinmap_power_regression_test.py
make safe CC="${ARM_CC:-clang}" OBJCOPY="${ARM_OBJCOPY:-llvm-objcopy}" OBJDUMP="${ARM_OBJDUMP:-llvm-objdump}"
TMP="$(mktemp -d)"; trap 'rm -rf "$TMP"' EXIT
checks=(
  'first_spin_safety_host_test.c sensorless_control.c vesc_ebics_flux.c'
  'foc_adc_contract_host_test.c foc_adc_contract.c stock_current_frontend.c'
  'foc_adc_motor_handoff_host_test.c foc_adc_motor_handoff.c sensorless_control.c vesc_ebics_flux.c'
  'motor_id_host_test.c motor_id.c sensorless_control.c vesc_ebics_flux.c'
  'sensorless_plant_host_test.c sensorless_control.c vesc_ebics_flux.c'
  'motor_core_host_test.c sensorless_control.c vesc_ebics_flux.c'
  'power_logic_host_test.c power_logic.c'
  'iap_ack_host_test.c iap_proto.c'
  'iap_transport_host_test.c iap_proto.c'
  'stock_adc_timing_host_test.c stock_adc_timing.c'
  'stock_adc_quality_host_test.c stock_adc_quality.c stock_current_frontend.c'
  'stock_adc_snapshot_host_test.c stock_adc_snapshot.c stock_current_frontend.c'
  'stock_dual_adc_runtime_host_test.c stock_dual_adc_runtime.c stock_dual_adc_plan.c stock_current_frontend.c'
  'test_g30_fixed_scan.c g30_fixed_scan.c'
  'test_stock_adc_mode_guard.c stock_adc_mode_guard.c stock_dual_adc_plan.c stock_current_frontend.c'
  'test_ble_motor_probe.c motor_probe.c ble_motor_probe.c'
)
checks+=('motor_pipeline_host_test.c motor_pipeline.c foc_adc_contract.c stock_current_frontend.c sensorless_control.c vesc_ebics_flux.c')
checks+=('stock_tim1_gate_host_test.c stock_tim1_gate.c')
checks+=('vesc_ebics_flux_host_test.c vesc_ebics_flux.c')
checks+=('g30_adc_current_window_host_test.c g30_adc_current_window.c stock_current_frontend.c')
checks+=('g30_adc_current_offsets_host_test.c g30_adc_current_offsets.c')
checks+=('drv126_adc_golden_host_test.c stock_current_frontend.c')
checks+=('motor_id_auto_host_test.c motor_id_auto.c motor_id.c motor_config_txn.c sensorless_control.c vesc_ebics_flux.c')
checks+=('motor_id_vector_output_host_test.c motor_id_vector_output.c sensorless_control.c vesc_ebics_flux.c')
checks+=('motor_id_adc_feedback_host_test.c motor_id_adc_feedback.c g30_adc_current_offsets.c')
for check in "${checks[@]}"; do
  read -ra files <<< "$check"
  test="${files[0]}"; args=()
  for f in "${files[@]:1}"; do args+=("src/$f"); done
  # Sensorless now calls the same physical-count model as ADC; avoid
  # duplicate objects for tests already naming stock_current_frontend.c.
  if [[ " $check " != *" stock_current_frontend.c "* ]]; then args+=("src/stock_current_frontend.c"); fi
  extra_flags=()
  if [ "$test" = sensorless_plant_host_test.c ] || [ "$test" = motor_pipeline_host_test.c ]; then
      extra_flags+=(-DSENSORLESS_CLOSED_LOOP_ALLOWED=1)
  fi
  gcc -std=c11 -O1 -g -fsanitize=undefined -Isrc "${extra_flags[@]}" "tools/$test" "${args[@]}" -lm -o "$TMP/${test%.c}"
  "$TMP/${test%.c}"
  echo "PASS: $test"
done
for flags in '-DPOWER_STAGE_ARM_ALLOWED=1 -DSENSORLESS_RUN_ALLOWED=0' '-DPOWER_STAGE_ARM_ALLOWED=0 -DSENSORLESS_RUN_ALLOWED=1'; do
  # shellcheck disable=SC2086
  if "${ARM_CC:-clang}" --target=arm-none-eabi -mcpu=cortex-m3 -mthumb -Isrc $flags -c src/main.c -o "$TMP/bad.o" >"$TMP/reject.log" 2>&1; then
    echo 'FAIL: unexpectedly permitted motor power build' >&2; exit 1
  fi
  grep -q 'DRV126 gate profile ready; powered build still requires valid ADC timing and current scaling' "$TMP/reject.log"
  echo "PASS: motor-power compile rejection $flags"
done
if [ "$(stat -c %s build/DeltaESC_G30D_v0_9_11_ble_syncsafe.bin)" -ge 53248 ]; then
  echo 'FAIL: exceeds G30 active application region' >&2; exit 1
fi
if command -v llvm-nm >/dev/null 2>&1; then
  llvm-nm build/DeltaESC_G30D_v0_9_11_ble_syncsafe.elf | grep -q 'motor_id_accept'
  llvm-nm build/DeltaESC_G30D_v0_9_11_ble_syncsafe.elf | grep -q 'motor_id_commit'
  if llvm-nm -u build/DeltaESC_G30D_v0_9_11_ble_syncsafe.elf | grep -E '__aeabi_(f|d|[ui]?ldiv)'; then echo 'FAIL: floating point / 64-bit division in Cortex-M3 link' >&2; exit 1; fi
fi
# Keep the complete symbol table in a file to avoid SIGPIPE with pipefail.
llvm-objdump -t build/DeltaESC_G30D_v0_9_11_ble_syncsafe.elf > "$TMP/v098_symbols.txt"
if ! grep -q 'g30_adc_current_flags' "$TMP/v098_symbols.txt"; then
  echo 'FAIL: ADC timing contract is not linked into Cortex-M3 ELF' >&2; exit 1
fi
if ! grep -q 'motor_id_auto_tick' "$TMP/v098_symbols.txt"; then
  echo 'FAIL: Motor ID auto sequencer not linked into Cortex-M3' >&2; exit 1
fi
if ! grep -q 'g30_adc2_current_offset_normalize' "$TMP/v098_symbols.txt"; then
  echo 'FAIL: ADC2 channel offset normalization is not linked into Cortex-M3 ELF' >&2; exit 1
fi
if ! grep -q 'case 0xF7u:' src/main.c; then
  echo 'FAIL: Motor ID read-only status missing' >&2; exit 1
fi
if ! grep -Fq 'const motor_id_auto_io_t id_io={motor_id_hw_vector_mv,motor_id_hw_gate_off,0};' src/main.c; then
  echo 'FAIL: Motor-ID vector_mv not wired to target firmware' >&2; exit 1
fi
if ! grep -Fq 'g_motor_id_vector_owner' src/main.c; then
  echo 'FAIL: PWM ownership arbitration missing' >&2; exit 1
fi
echo 'PASS: v0.9.11 Motor-ID vector_mv to stock TIM1/ADC actuator (source-only, no hardware enable)' 

if [ -n "${DRV126_STOCK_BIN:-}" ]; then
    python3 tools/audit_drv126_adc_stock.py "$DRV126_STOCK_BIN"
fi

# Strict v0.9.11 integration checks: MCU event IRQ, captured real JDR, epoch,
# no unqualified build and no invented flux feedback.
grep -Fq '[16u + IRQ_TIM1_UP] = TIM1_UP_IRQHandler' src/main.c
grep -Fq 'motor_id_adc_feedback_queue(&g_motor_id_feedback' src/main.c
grep -Fq 'motor_id_adc_feedback_capture(' src/main.c
grep -Fq 'motor_id_auto_tick(&g_motor_id_auto,t0/' src/main.c
grep -Fq 'fb.command_epoch==g_motor_id_auto.command_epoch' src/main.c
grep -Fq 'COMM_CURRENT_SCALE_HW_VALID 0u' src/main.c
grep -Fq 'COMM_ADC_TIMING_HW_VALID 0u' src/main.c
printf 'PASS: v0.9.11 TIM1 UEV to ADC coherent Motor-ID feedback integrated, motor gates remain disabled\n'
