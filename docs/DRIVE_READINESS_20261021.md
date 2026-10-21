# DeltaESC: source audit and remaining work for driving

Date: 21 October 2026. Task: finish DeltaESC for the owner's G30D Gen1.
Contract: [IntheLoopRemoteLab PROJECT_CONTRACT v1.0](https://github.com/ZachWay9bot/IntheLoopRemoteLab/blob/main/PROJECT_CONTRACT.md).
Handoff read: `PROJECT_HANDOFF.md`, Git blob `7b9c3502df6899a6cbdb05f95e29b0d915ff93f6`.

## Exact reference

- Repository main commit: `eb30fbfd61fba4768be399a3aedf98dc2c2039f0`.
- Source: `downloads/DeltaESC_G30D_v0_10_2_COMPLETE_SOURCE.zip`.
- Source archive SHA256: `3a7a4533c743df97f555a199ef603ff52efb34af752301a43ffb0497a3285f6f`.
- Embedded image: `build/DeltaESC_G30D_v0_10_2_active_telemetry_rc.bin`, 32,236 bytes.
- Image SHA256: `2b6e26b656a25bdab2b2e98eca447452b66628208cecf956e0df01134cf9eb75`.
- `FW_BUILD=0x1020`. Existing Makefile's `rolling-active-rc` still writes the filename `v0_10_1`; its contents are built from the current v0.10.2 sources. The original workflow copied this output to the v0.10.2 name.
- Target controller: original G30D Gen1 3-cap STM32F103. This is separate from the destroyed potted G30-identical controller. No target controller or active target test interface is available in this task. Historical ST-Link access is not a current recovery test.

## What this change does

Only adds an audit runner, copies four existing C tests from development commit `607059f7fa112e9ca04a63d2b6ccddb5cf82354e`, and adds a software CI job. The four tests come from `v0.10.0/tools/` and `v0.10.1/tools/`; their tested implementations come from the current source ZIP.

No firmware source, image, flash routine, bootloader, GPIO, motor limit, protection mechanism, release hold or existing CI gate is changed. There is no hardware operation, motor model, new simulator, release or private queue change.

## Findings in the actual source

| ID | Finding and source | Effect | Evidence type |
|---|---|---|---|
| D1 | `main.c:app_write_action`, E5 evaluates `commissioning_guard_mask() & ~COMM_GUARD_THROTTLE_NOT_IDLE`. `commissioning_guard_mask()` supplies `COMM_CURRENT_SCALE_HW_VALID=0` and `COMM_ADC_TIMING_HW_VALID=0`, including in the rolling RC. | With all other prerequisites satisfied, bits `0x1000` and `0x4000` still reject every E5 start. The RC exceptions in `motor_hw_arm()` cannot be reached through E5. | Original `commissioning_guard_eval()` and verbatim E5 predicate compiled and evaluated by the audit. |
| D2 | `main.c:gate_safety_check` requires `MOTOR_ID_BENCH_ALLOWED && g_id_bench_requested` for any armed bridge. `rolling-active-rc` defines `MOTOR_ID_BENCH_ALLOWED=0`. | A correctly configured armed rolling RC would be terminally disarmed by this existing runtime predicate. This is downstream of D1; it is not a claim that the current image successfully arms. | Verbatim predicate compiled with the RC's flag and otherwise expected CCER/BDTR values. |
| D3 | `motor_pipeline_input()` sends brake and throttle-idle to `motor_pipeline_stop()`. Its actual gate-off callback is `motor_hw_gate_off()` -> `power_stage_force_disarm()` -> `g_gate_kill_latched=1`. `motor_hw_arm()` refuses this latch. | Ordinary release of throttle or braking prevents another start until reset. Pure pipeline tests use a callback that does not contain this target latch, so their success does not cover this integration behavior. | Trace through unchanged original functions, with existing pipeline stop tests as component evidence. |
| D4 | The only active start call is in authenticated BLE E5. Dashboard ADC handling calls `motor_pipeline_input()`, which stops or refreshes input but does not start or vary `run_iq_counts`. Rolling torque is capped at 12 ADC counts in `sensorless_control.c`. | This is an explicit bench motion request with a bounded fixed setpoint, not a complete throttle-driven vehicle application. | Source inspection. |
| D5 | `motor_pipeline_input()` stops on brake; there is no negative torque/brake-current request. `dashboard_safe_status_payload()` always returns battery 50%, speed 0, fault 0. | Electrical braking and live dashboard status are not implemented for driving. A passing dashboard test currently confirms the placeholders. | Source inspection and existing dashboard/pipeline C tests. |
| D6 | Active startup hardcodes donor R=583000 uOhm, L=640000 nH, flux=12100 uWb, frame offset=21327. Passive BEMF uses 15 pole pairs, circumference 763 mm and raw-forward-step 5. | These are not measured G30 Gen1 values. They affect observer, speed threshold and handover; choosing replacements without target measurements would be guessing. | Source compared with the current handoff's explicit E-bike donor provenance. |
| D7 | Committing a staged F3 phase offset through F5 changes `ctrl.phase_offset_u16`, but `g_rolling_bemf.cfg.phase_offset_u16` is set separately by startup. The seed and observer expect the same frame. | A later measured phase-offset configuration does not currently update both users of that offset. Scope and correction belong with the target motor-profile work. | Source inspection; no claim of a hardware failure. |

D1/D2/D3 are important software blockers, not missing documentation. They must not be hidden by a successful component test or build. Any change to start policy, normal-stop behavior or existing motor-safety conditions must be discussed with the owner under the contract before implementation.

## Software verification

Run from the repository root:

```bash
python3 tools/audit_drive_source.py \
  --source-zip downloads/DeltaESC_G30D_v0_10_2_COMPLETE_SOURCE.zip \
  --output audit-results/drive-source-audit.json
```

Add `--build-arm` where `clang-18`, `llvm-objcopy-18`, `llvm-objdump-18` and make are available. CI does this on Ubuntu 24.04. It builds the unchanged existing target and records size, hash and comparison with the reference BIN. It never exports a new flash package.

The runner executes 33 C test groups against the extracted firmware source with GCC and undefined-behavior checks. It excludes the old synthetic PMSM plant. Existing tests with fixed injected inputs are component checks, not electrical or hardware evidence. It separately reproduces D1 and D2 and traces D3; successful reproduction means the blockers exist, not that the firmware is ready. The result JSON always records `drive_ready=false` and `hardware_tested=false` for this audit.

Historical `run_v114_tests.sh`, `run_v102_tests.sh`, README and some static grep checks refer to older source stages and old output names. They are not direct current-release acceptance runners. No obsolete condition was relaxed in those files; the added runner builds the original tests with their actual dependencies.

Local verification: 33 C test groups PASS, D1/D2 reproduced, D3 source trace confirmed. The initial ad-hoc batch omitted the shared count-model object in two invocations; rerunning those with the original runner's dependency resolved both linker errors. Those were audit invocation errors, not firmware defects. ARM results are tracked separately in CI, not implied by the native tests. Firmware `src/` hashes are checked before and after each audit run.

## Concrete next implementation package — owner decision needed

1. Repair the inconsistent RC start/runtime checks without marking unmeasured calibration as valid or removing overcurrent, ADC-window, deadtime, brake-priority or emergency checks. Benefit: the existing explicit bench entry can become internally consistent. Risk: repairing its reachability permits real energization once other conditions are met; hardware validation remains necessary. Alternative: leave the reference unchanged and unable to complete this start path.
2. Define normal coast versus terminal fault/explicit E6 stop. Benefit: ordinary throttle release can allow a later neutral/rolling restart. Risk: this intentionally changes the existing stop/rearm policy. Preserve terminal emergency latching. Alternative: retain the one-shot bench behavior.
3. Implement proportional throttle demand and the vehicle's normal rolling-start request through the existing dashboard protocol. Preserve brake priority and do not invent higher current limits. Benefit: independent operation from normal controls. Risk: new behavior must be validated with the target motor and fixed current envelope. Alternative: retain BLE E5 bench-only operation.
4. Use actual G30 Gen1 measurements to supply R/L/flux, pole-pair/geometry and electrical phase/direction; keep both phase-offset users consistent. No values are fabricated and no donor profile is silently renamed to G30.
5. Electrical braking and live speed/battery/fault telemetry need their own implementation and target test. Regenerative current and pack-voltage limits require actual pack/controller data; no brake current or voltage ceiling is chosen in this audit.

These are proposed corrections, not completed firmware changes or a new safety architecture. Bootloader/IAP/recovery work is outside this package.

## Hardware still required before driving

| Test | Why required | Acceptance evidence |
|---|---|---|
| Confirm the original G30 target, motor wiring and available existing flash/recovery path | Prevent mixing controller/motor identities | Physical target and measured setup documented before active work |
| Measure target R/L/flux, electrical cycles per wheel turn, direction and phase frame | Observer and speed/start thresholds must match this motor | G30 logs with software identity and measurement method; never donor-only evidence |
| Measure current zero/gain/sign, bus-voltage scale and TIM1 gate/deadtime/ADC timing | Raw software counts and source-level register checks do not prove physical current or sampling aperture | Oscilloscope and current/voltage reference results on the exact target |
| Test wheel-free rolling takeover, throttle modulation, release/restart, brake, lost input and observer/current fault stops | Validate the full application path and corrected stop policy | Exact BIN hash, input traces and observer/current/fault logs for each test |
| Validate electric braking and then controlled-load behavior | Bench takeover cannot establish loaded driving or braking | Measured current/voltage/temperature, repeatable torque/brake response and controlled stop |

No powered test, flash, recovery qualification or road-readiness claim is made. Existing release holds remain in force. The task can continue in source/build work after the owner decides the concrete safety/start/stop changes; completing actual driving requires the physical target.

## Completed CI evidence and handoff

- Audit implementation commit: `d04e06797d13693ad096c8432db98714667118ac`, branch `audit/drive-readiness-20261021`, [draft PR #20](https://github.com/ZachWay9bot/DeltaESC-G30D/pull/20).
- [Source/ARM CI run 39519770909](https://github.com/ZachWay9bot/DeltaESC-G30D/actions/runs/39519770909), completed 21 October 2026 at 02:11 Berlin time: **PASS**. Pull-request merge test commit `d6c7f7ac3f07af1b03f1eb85844e8f3dbb226a25` combines the audit commit with the recorded unchanged main baseline.
- 33 original C test groups PASS, D1/D2 reproduced and D3 traced. Compiler family: Ubuntu Clang 18.1.3 / GCC 13. The ARM rebuild is **32,236 bytes, byte-identical** to the reference BIN; SHA256 `2b6e26b656a25bdab2b2e98eca447452b66628208cecf956e0df01134cf9eb75`.
- [Existing UART safety audit 39519770911](https://github.com/ZachWay9bot/DeltaESC-G30D/actions/runs/39519770911): **PASS**, including preservation of its current release hold. This is a source/policy check, not a physical restore test.
- Approved work completed: source provenance, native tests, integration diagnosis, unchanged ARM rebuild, versioned findings and concrete correction proposal. No new firmware behavior was implemented.
- Pending owner decision: the specific start/runtime, normal-stop/rearm and throttle-behavior corrections listed above. These affect contract-protected start/safety policy; blanket removal of qualification bits is not authorized.
- Next three steps after that decision: implement only the agreed source corrections and their integration tests; build and review the resulting exact image without issuing a flash release; obtain the target G30 measurements and agree a separate physical test package.
- Stop criteria: unapproved motor/stop-policy or flash/recovery change, unknown target/motor identity, missing required physical action or evidence. No active RemoteLab session, motor operation or use/change of the private XBOT queue occurred.
