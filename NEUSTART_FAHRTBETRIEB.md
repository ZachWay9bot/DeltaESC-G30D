# DeltaESC G30D – aktiver Neustart Fahrbetrieb (23.10.2026)

## Arbeitsbasis

- **Dieser Branch** ist direkt vom **release/v0.10.2-active-telemetry-rc** abgezweigt, Basiscommit `431b8802196d5d18da4e584d7f380212c99b022c`.
- Der alte Entwicklungszweig wird **nicht** als neue Firmwaregrundlage benutzt und es werden daraus keine pauschalen Änderungen übernommen.
- DeltaESC v0.10.2 ist eine **kompilierte, softwaregeprüfte RC** auf G30D Gen1, aber noch **kein auf G30-Hardware bestätigter Fahrstand**.
- Reale historische Motorlauf- und anschließende IAP-Update-Erfolge existieren mit **XBOT FO76**. Diese sind **Referenz für die funktionierende Entwicklungssequenz**, nicht als Bytegleichheit oder G30-Motorvalidierung des DeltaESC-Images zu deklarieren.

## Fest vereinbarte Zielkonfiguration

- Ziel: G30D Gen1, sensorloser FOC, proportionale Gasannahme über Stock-Dashboard, normaler Coast-Stopp und Wiederanlauf.
- Kein regeneratives oder aktives Bremsmoment; Bremse signalisiert nur Coast.
- Der erwartete Lieferzustand ist DRV 1.8.7, **DRV126 ist die geplante Basis**, nicht schon als vorgefunden anzunehmen.
- **Ausschließlich bestehender IAP-Weg**. Auf dem Ziel ist ST-Link physisch nicht erreichbar.
- Bootloader, Flash-Layout, IAP-Rückweg, Versions-/CRC-/Flashgrößenprüfungen **nicht eigenmächtig ändern**. Keine zusätzlichen Sperren einbauen.
- Bewährten Code nur bei konkret begründetem Bedarf verändern. Motorfreigabe für Softwareentwicklung ist erteilt; kein autonomer Flashauftrag.

## Arbeiten bis zum Fahrbetrieb

1. Die **unveränderte** v0.10.2-Quelle reproduzierbar bauen und die bestehende Motor-/IAP-Schnittstelle als Baseline fixieren. Kein späteres experimentelles Firmwarepaket als Ausgangscode mischen.
2. Nur für den Fahrbetrieb erforderliche Korrekturen einzeln isolieren und in Hosttests prüfen: Gas/Neutral, Coast/Wiederanlauf, Dashboard-Timeout und Rolling-Handover.
3. Auf der passenden echten G30-Hardware Strom-/VBUS-Skalierung, Sensorless-Phasenbezug, Drehrichtung und Motorparameter bestimmen. **Donor-E-Bike-Motorwerte nicht blind als G30-Werte einsetzen.**
4. Wheel-free mit strombegrenzter Versorgung den tatsächlichen FOC-Übergang nach Rollen, Gasregelung, Coast und erneuten Start messen; dabei bestehende Schutzfunktionen nicht lockern.
5. Die Rückkehr zur Stock-Firmware über den vorhandenen IAP-Weg mit derselben Hardware belegen. **Keinen unbewiesenen neuen Flash auslösen.**

**Nächster messbarer Erfolg**: Motor dreht kontrolliert an echter G30-Hardware; danach erneuter IAP-Zugriff weiterhin möglich. Keine weitere Grundsatzanalyse als Ersatz für diese beiden Tests.

## Status

24.10.2026: Original-v0.10.2-Quellbaum unter `g30/` byteverifiziert importiert. Für den **reinen G30-Fahrpfad** wurden gezielt `main.c`, `motor_pipeline.[ch]`, `dashboard_runtime.[ch]` und `Makefile` sowie `drive_runtime.[ch]` und `g30_target_profile.h` mit den zuvor hostgetesteten Fahrkorrekturen übernommen. Keine IAP/Bootloader/UART/Linker-Änderung und kein SHU-Build. Validierte CI: [G30 clean drive and dashboard regression 40032651629](https://github.com/ZachWay9bot/DeltaESC-G30D/actions/runs/40032651629), **alle Schritte PASS**: unveränderte IAP/Linker-Bytes per `cmp`, native Gas-/Coast-/Rearm- und Dashboardtests, tatsächliche `main.c`-Integration einschließlich IAP-Parser, Cortex-M3 SAFE und DRIVE Link/BIN. SHA256 der erzeugten **unqualifizierten** Drive-BIN: `7b6a17d4df105ae3cf078dccb25ce926bce97fb46902317a812484e9b621bbb1`. Kein physischer Motor- oder Flash-Test. Das separate, nicht benötigte Motor-ID-CI-Ziel wird zur Fahr-CI nicht mitgebaut und in der Firmware selbst nicht verändert.

**Nächster Schritt mit Hardware:** exakte G30-Gen1-Motor- und Sensorwerte und IAP-Zustand messen/prüfen. Die eingebauten G30-Profilwerte bleiben bewusst 0 (unbekannt), bis Daten existieren. Keine donor-derived Profile vortäuschen, keine bereits erfolgte Fahrfreigabe behaupten.


## VERBINDLICHE OFFLINE-ABNAHME 24.10.2026

**Firmware/Baseline-Freeze: PASS** — GitHub Actions [Run 40036462792](https://github.com/ZachWay9bot/DeltaESC-G30D/actions/runs/40036462792), gesamter Workflow inkl. Schutzdateivergleich, Cortex-M3 safe/drive, realer main.c-Hosttests, Rolling-Edge-Cases, PowerShell-Dry-Run und Quell-/BIN-SHA-Manifest erfolgreich. Persistentes CI-Artefakt **DeltaESC-G30-one-time-offline-freeze** (ID `12537435969`, Ablauf GitHub 2027-01-21). Enthält Quellarchiv, `g30/`-Firmware-BIN, SAFE-BIN, Symbol-/Map-Dateien, `manifest.json`, `SHA256SUMS.txt`, `READ_BEFORE_FLASHING.txt`. **DRIVE-BIN SHA256** `7b6a17d4df105ae3cf078dccb25ce926bce97fb46902317a812484e9b621bbb1`; **SAFE-BIN SHA256** `5190ceb011a8cbd4cd8e461985ecefbeaa47a745f00ba38f41863429ee248eac`. Die DRIVE-BIN ist **softwaregebaut, nicht freigegeben zum unprueften Flashen**.

**One-pass BENCH EXTRAS: PASS** — GitHub Actions [Run 40044128202](https://github.com/ZachWay9bot/DeltaESC-G30D/actions/runs/40044128202), Tests der neuen RX-Diagnosedekodierung, fragmentierter Frames, CRC, G30-Konfigurationsframes und PowerShell DryRun. CI-Artefakt **DeltaESC-G30-read-only-bench-tools** (ID `12540790876`, Ablauf GitHub 2027-01-22). Liegt unter `bench/`; wurde nach dem Firmware-Freeze ergänzt, der zuvor bestandene Motor-Build wurde deshalb **nicht neu gestartet**.

**Nur physisch zu klaeren (nicht erneut offline testen):**
1. Istidentitaet und tatsaechlicher IAP-Rueckweg des ankommenden Controllers, **keine ST-Link-Annahme**.
2. G30-Gen1-Motorprofil: 15 bestaetigte Polpaare, echte R/L/Flux- und Phasenlage, Stromwandler/ADC/TIM1-Hardware, korrekte BEMF-Richtung.
3. Ein kontrollierter Wheel-Free-Motorlauf mit der gemessenen Konfiguration und dokumentiertem Coast/Restart/IAP-Zugriff.

**Kein weiteren Software-Testlauf ohne geaenderten Motorcode oder konkreten Fehlerbefund ansetzen.** G30-Diagnose bei Ankunft: erst `Identify`, bei nachgewiesener DeltaESC-App `Snapshot`, danach `G30_Decode_Log.py`. Motorstart ausschliesslich nach konkreter Freigabe und erfuellten Hardwarevoraussetzungen. Kein neuer Flash-/Recovery-/SHU-Mechanismus.

## Hardware angekommen / Arbeitsauftrag weitergeführt (24.10.2026)

**Benutzer bestätigt ausdrücklich denselben G30D-Gen1-Controllertyp wie im bisherigen Firmware-/IAP-Aufbau.** Keine neuen hypothetischen Hardwarevarianten oder ST-Link-Erfordernisse einführen. Die bisherigen FO72–FO76-IAP-/Motor-Bench-Erfolge sind real und werden NICHT nochmals offline simuliert. Bei der Beweiszuordnung muss jedoch unterschieden werden: Die alten FO7x-Motorparameter gehören zum 250-W-Donormotor; der jetzt angeschlossene Motor ist der echte **G30 Gen1**.

**ECHTE neuen Targetdaten:** UART COM9, Ninebot READ 0x10 antwortet mit Werks-Identität, READ 0x1A = `87 01` = **DRV 1.8.7**. Nicht als bereits installierte DRV126 oder DeltaESC ausgeben. Kein Flash bisher.

**Konkrete Softwarearbeit seit Ankunft:** Im Branch ist der **einmalige gemessene G30-Gen1-DRIVE-Build** unter `bench/G30_Gen1_Measured_Build.py` implementiert; der nimmt nur reale G30-Werte entgegen, ruft isoliert den vorhandenen `g30/Makefile drive-runtime` mit C-Profilmakros auf und erzeugt BIN/ELF/SHA-Manifest. Keine Firmware-/IAP-/Linker-/Flash-/Gate-Codeänderung. CI-Test und **tatsächlicher Cortex-M3-Firmwarebuild mit rein synthetischem Fixture** erfolgreich: [GitHub Actions 40124672586](https://github.com/ZachWay9bot/DeltaESC-G30D/actions/runs/40124672586); **Testimage wurde nicht veröffentlicht** und ist kein echter Fahrbeleg.

**Noch konkret zu tun, ohne Protokolle zu wiederholen:** G30-spezifische `R/L/Flux/phase_offset/wheel_circumference` einmalig aus geeigneten realen Messungen gewinnen oder aus einem klar G30 zugeordneten bestehenden Messlog importieren; danach exakt einen profilierten DRIVE-Build produzieren und den überwachten Motorlauf auf tatsächlicher Hardware auswerten. 15 Polpaare sind bekannt. Der Default-Drive-BIN mit Nullprofil fährt **absichtlich nicht**. Keine FO70-/FO76-Donorkonstanten als G30-Messungen ausgeben; keine unqualifizierte, kopierte BIN als „fahrbereit“ deklarieren.
