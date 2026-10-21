# DeltaESC handoff — 21 October 2026

## Current instruction, superseding the earlier approval interpretation

The owner explicitly authorized motor enable and all development necessary
for driving. Do not ask again for individual motor/start/coast/control
changes. The overriding condition is reversibility **without ST-Link**.
There is **no ST-Link option on this target**. Do not carry older SWD/ST-Link
assumptions forward as available recovery. See the dated clarification in
`../PROJECT_CONTRACT.md`; the private XBOT queue is untouched.

## Source and work completed

- Reference: main `eb30fbfd61fba4768be399a3aedf98dc2c2039f0`, v0.10.2
  FW_BUILD `0x1020`, complete source ZIP in `downloads/`.
- Initial audit: PR #20, source/ARM test commit
  `d04e06797d13693ad096c8432db98714667118ac`, byte-identical ARM rebuild
  and 33 original C groups passed. That audit's owner-decision and ST-Link
  wording is superseded by this handoff and the current user instruction.
- Development branch: `dev/drive-reversible-20261021`.
- New complete source tree: `firmware/`, v0.10.3 FW_BUILD `0x1030`.
- Fixed the RC commissioning-mask versus arm-backend contradiction and the
  runtime E7-only owner check. Both start entry points use the same atomic
  motor-start function with IAP precedence and existing qualification bits.
- Added a separate normal coast callback, retaining terminal emergency
  callbacks for E6 and real faults. Real target C integration checks normal
  throttle stop/restart and brake priority, not just a permissive fake
  pipeline callback.
- Added proportional gas, existing RC current limits retained. Added live
  speed/fault status; battery is still a placeholder and braking is coast.
- New target does not seed donor R/L/flux or wheel geometry. Existing
  transactional motor configuration remains; FF adds RAM geometry/readback.
  Shared phase offset updates both observer and passive seed. F6 retains
  its original read-only ADC diagnostic role.
- All IAP/UART source, CPU register header and linker bytes are identical
  to the reference. Native tests run the unchanged IAP feed/parser and
  protocol-order-error path without flash erase/program.
- Local 35 component/integration groups passed. ARM and CI results must be
  read for the exact development commit; do not infer them from the older
  audit. No hardware test, flash, update/restore cycle or image release has
  taken place in this development work.

## Still required, by priority

1. Identify the target's currently installed image and the existing actually
   reachable stock-return transport. A handler within the app is not proof
   of recovery when the app cannot start. Existing source holds are not
   runtime locks and have not been removed or added to IAP.
2. Record a real stock-return procedure and its evidence without relying
   on unavailable ST-Link. No new bootloader is installed, no flash geometry
   is guessed, and no active test is queued under an expired/other project
   session. If there is no independent entry, state that limitation directly.
3. Obtain measured G30 R/L/flux, wheel electrical cycles/pole pairs,
   circumference, direction/frame, current/voltage scaling and ADC/gate
   timing. Donor values and generic serial-number guesses are not measured
   target data. Native configuration fixtures are not motor measurements.
4. Validate wheel-free takeover, normal gas response, release/restart and
   all existing terminal fault stops on the exact candidate, then controlled
   load. Current limits remain the original low-current RC envelope.
5. Integrate genuine BMS/SOC and electrical braking after target battery
   voltage/current constraints are established; current candidate coasts on
   brake and is not ready for driving.

No further motor-development approval is missing. Actual physical setup,
rollback evidence and target measurements are missing. Continue source work
within that scope without manufacturing a hardware success or adding IAP
eligibility gates.
