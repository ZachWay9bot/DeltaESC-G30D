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
