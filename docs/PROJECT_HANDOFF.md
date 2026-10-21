# DeltaESC handoff — 21 October 2026

## Current instruction, superseding the earlier approval interpretation

The owner explicitly authorized motor enable and all development necessary
for driving. Do not ask again for individual motor/start/coast/control
changes. The overriding condition is reversibility **without ST-Link**.
There is **no ST-Link option on this target**. Do not carry older SWD/ST-Link
assumptions forward as available recovery. See the dated clarification in
`../PROJECT_CONTRACT.md`; the private XBOT queue is untouched.

## Korrektur: bestaetigte Hardwarehistorie und aktueller Lieferplan

ST-Link ist physisch ausgeschlossen. Bis auf Weiteres: IAP; erwarteter
Lieferstand DRV 1.8.7, geplante Basis DRV126. Der Controller ist noch nicht
eingetroffen. Keine erneute Frage nach ST-Link oder Motorfreigabe.

Die historischen Original-Logs wurden erneut gelesen: FO76-Flash,
FO76-Motor/Wizard und anschliessender IAP-Weiterflash aus FO76 sind real
bestaetigt. FO91/0.9.1 wurde danach installiert; die Rueckkehr zu FO80
scheiterte mit IAP-Status 7. Die damalige CRC-/128-KiB-IAP-Bedingung ist
als Regression dokumentiert. "Kein Hardwaretest" unten bezieht sich
ausschliesslich auf die neue v0.10.3-Arbeit.

**Wichtige Artefaktzuordnung:** Die real geflashte FO76-Datei hatte
21.712 Bytes / SHA256 `35ceae5c635c66f667380fc8dd0777463aed5dde98014328d123be5965812554`;
das spaeter reproduzierte FO76-Source-Paket liefert 21.728 Bytes /
`a8b7ca7d45fdfa32cbcbfdfbe98e2e655a266ecc57aea62f8149ecf3cbe74bcb`.
Gleiche Firmwarekennung beweist daher keine Bytegleichheit.
[Originalbelege und Regression](HARDWARE_HISTORY_FO76_FO91_20261021.md).

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

## Verified source/build result

- Exact software-tested source commit:
  `cc7df57d10da9d16c1e8387cd8622bf327e364bd`.
- [CI run 39573629137](https://github.com/ZachWay9bot/DeltaESC-G30D/actions/runs/39573629137):
  **PASS**, 35 C/component/integration groups, unchanged IAP/transport/linker
  byte checks, existing anti-lockout audit, three normal ARM configurations
  and a fourth separate configured-profile fixture build.
- Drive candidate: 32,748 bytes; SHA256
  `707c17285e65bfb94074414a97f11f71dd57e401930e2aaa28532b3aadddd950`.
  It remains unqualified for flashing/driving. Fixture binaries are not
  offered as target images and no test values are inserted into this BIN.
- `src/g30_target_profile.h` provides a measured built-in profile for eventual
  standalone startup without new persistent flash/config writes. Unknown
  target fields are zero. Native and ARM tests cover an explicit fixture
  separately; those numbers are not G30 measurements. RAM F0-F5/FF is still
  available, and the original F6 ADC page is unchanged.
- The original DRV126 full dump was actually reread and hash-matched. A
  limited boot-routine inspection verifies the staging-copy/app-handoff
  role but supplies no independent UART rescue proof. See
  [bootstrap audit](DRV126_BOOTSTRAP_AUDIT_20261021.md). The private dump is
  not published. Its identity is not proof of the current target's loader.
- Current central contract is v1.1. The motor authorization and unavailable
  ST-Link correction are persisted on main in both project documentation
  and the shared handoff; older contradictory instructions are historical.
- Subsequent documentation changes do not change the above tested source.

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
