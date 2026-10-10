# DeltaESC G30D – Motor-Bench RC (E7, echte TIM1-Ausgänge)

**Entwicklungsstand. Nicht am G30D-Motor getestet. Kein Fahr-Release und keine
Freigabe für 10S-Akku-Erstversuche ohne unabhängige Strom-/Energiebegrenzung.**

Ziel ist ausschließlich die seit v0.9.14 vorhandene motorfähige R/L/Flux-
Firmware endlich mit den bisher getrennten v0.9.16/v0.9.17-
Gate-Schutzkorrekturen zusammenzubringen, **nicht eine weitere App**.

## Was hier anders ist als die Source-only-Builds

- Rekonstruktion des bereits CI-geprüften v0.9.14:
  authentifiziertes **E7** und **E6**, TIM1 CH1/2/3 + CHxN, ADC1/2,
  4-kHz-Motor-ID-Sequenz und F7/F8/F9-Status.
- Übernahme der v0.9.16-Elektronik-Hypothesen: GPIO-Pulldowns statt
  Eingang-Pullup, AOE=0 (kein automatisches MOE-Rearm), keine erfundene
  BKIN-Polarität, Register-Preinit und Readbacks.
- Übernahme der v0.9.17-Notabschaltung: E6, Abbruch, Fehler, Watchdog und
  Poweroff setzen einen **dauerhaften Gate-Kill-Latch**, MOE=0,
  CCER-Motorbits aus; erst ein MCU-Neustart setzt den Latch zurück.
- **Korrektur des bisher entscheidenden Fehlers:** Ein normaler
  4-ms-Pausenabschnitt in der L-Messung führt stattdessen zu
  `motor_id_bench_coast()`: CCER=0x1000, alle sechs externen
  Ausgänge in Eingang-Pulldown, MOE bleibt nur für den CH4-ADC-Trigger
  gesetzt. Nach einem Fehler darf diese Routine **nicht** rearmen.
- Der Gate-Sicherheitscheck unterscheidet passives TIM1-Profil (CCER=0x1000)
  und E7-Motorprofil (CCER=0x1555), jeweils mit BDTR=0x8C40 als
  reinem Registervertrag. Die v0.9.16-Passivprüfung hatte **alle**
  motoraktiven CCER-Zustände ausgeschlossen und muss im E7-Profil
  deshalb bedingt sein.
- Normales `E5` / Sensorless-Fahrbetrieb ist **compile-verboten**.
  Kein SHU-IAP-Umbau. Kein neuer Android-Code.
- E7-Build meldet `FW_BUILD=0x0914` **ausschließlich für den unveränderten
  Android-0.3.6-Guard**. Das ist nicht die alte v0.9.14-Binärdatei;
  zur Identifikation dient der neue Dateiname plus SHA256 des
  GitHub-Actions-Artefakts.

## Reproduzierbare Motor-Bench-Builds

GitHub workflow `.github/workflows/motor-rc-source-audit.yml`:

1. 0.9.0-Basisartefakt nach SHA256 geprüft rekonstruieren; alle Patches
   0.9.1 bis 0.9.14 und die 26 bestehenden Testgruppen laufen.
2. v0.9.16 und v0.9.17 Sicherheits-Overlays anwenden.
3. `python3 motor-rc/tools/apply_motor_rc.py source`
4. 24 normale E7-Abschalt-/Rearm-Wiederholungen simulieren, E6-Latch
   prüfen und alle 65536 Varianten des BDTR-Gate-Vertrags abdecken.
5. `make safe id-bench` mit Clang 18 Cortex-M3 und LLVM bintools.
6. E5/Normal-Drive-Kompilierung explizit als Fehler verlangen.
7. Artefakt `DeltaESC-G30D-MOTOR-RC-E7-EXPERIMENTAL-NOT-HW-VALIDATED`
   enthält separat die SAFE- und `motor_id_bench.bin`-Datei.

## Für den konkreten Original-ESC noch offen

- Exakter elektrischer Zustand aller MT8006A-Eingänge beim CH1/2/3-Off;
  STM32-CCER/MOE-Readbacks ersetzen keine FET-Gate-Messung.
- Externe Hardware-Abschaltung / Gate-Fault bei vergossener Endstufe.
- Stromwandlungsfaktor und ADC-Validität unter realem Motorstrom.
- Vollständige G30-Power-Latch-Route (früherer Controller-Brick).
- E7-R/L/Flux wirklich am Rad frei durchgeführt; Übergang in Fahr-FOC.

**Das Binary ist ein echter kompilierter E7-Motor-Codepfad, aber kein
nachweislich sicherer Firmware-Flash. Ein unveränderter Ninebot-10S-Akku und
BMS sind allein keine ausreichende Absicherung für den ersten aktiven
Leistungsstufentest.**
