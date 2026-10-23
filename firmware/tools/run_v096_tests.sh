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
for check in "${checks[@]}"; do
  read -ra files <<< "$check"
  test="${files[0]}"; args=()
  for f in "${files[@]:1}"; do args+=("src/$f"); done
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
if [ "$(stat -c %s build/DeltaESC_G30D_v0_9_6_ble_syncsafe.bin)" -ge 53248 ]; then
  echo 'FAIL: exceeds G30 active application region' >&2; exit 1
fi
if command -v llvm-nm >/dev/null 2>&1; then
  llvm-nm build/DeltaESC_G30D_v0_9_6_ble_syncsafe.elf | grep -q 'motor_id_accept'
  llvm-nm build/DeltaESC_G30D_v0_9_6_ble_syncsafe.elf | grep -q 'motor_id_commit'
  if llvm-nm -u build/DeltaESC_G30D_v0_9_6_ble_syncsafe.elf | grep -E '__aeabi_(f|d|[ui]?ldiv)'; then echo 'FAIL: floating point / 64-bit division in Cortex-M3 link' >&2; exit 1; fi
fi
if ! grep -q 'case 0xF7u:' src/main.c; then
  echo 'FAIL: Motor ID read-only status missing' >&2; exit 1
fi
echo 'PASS: source-only software checks (not a hardware/flash release)' 
