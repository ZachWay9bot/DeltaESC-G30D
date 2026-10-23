# DeltaESC v0.10.3 – Fahrlogik / Software-Abnahme (21.10.2026)

## Verbindlicher Geltungsbereich
- Entwicklung: `dev/drive-reversible-20261021`, Quellcode-Commit **`26629227ad7cc882fb3f68426153c83767ce5e3e`**.
- Motorfreigabe innerhalb der vereinbarten Softwareentwicklung erteilt; KEIN Auftrag zum Flashen, Motoransteuern oder Aendern von IAP/Bootloader/Recovery.
- Zielcontroller ohne ST-Link-Zugang; FO76 ist historische, real bestaetigte Flash-/Rueckflash-Referenz. DeltaESC v0.10.3 ist **nicht** FO76 und war nicht auf dem realen Zielcontroller installiert.
- Alle IAP-/UART-/Linkerdateien sind unveraendert gegen die separate v0.10.2-DeltaESC-Quelle. Daraus folgt **keine** behauptete Bytegleichheit der gesamten Firmware oder ein real bestaetigter Rueckflash unter v0.10.3.

## Fortschritt: Dashboard-Watchdog und atomare Brake-Coast-Uebergabe (21.10.2026)

- `motor_pipeline_watchdog()` laesst auch waehrend Coast eine frueher gemerkte neutrale Gasstellung ablaufen, sobald frische 0x65-Eingaben ausbleiben. Die erlaubte Startsequenz nach Empfangsausfall bleibt: neuer Gas-null-Frame -> neuer Gasbefehl -> BEMF/3-km/h-Rollstart.
- Bei bereits aktivem Motor bleibt ein Dashboard-Timeout **terminal**. Er darf nicht durch Status/Heartbeat oder nachtraegliche Gasbefehle unbemerkt rueckgaengig werden.
- `main.c:handle_stock_dashboard()` uebernimmt Gas, Bremse, Null-Gas-Qualifikation und Coast-Stopp unter derselben kurzen Interruptsperre mit Wiederherstellung des vorherigen PRIMASK. Damit kann ein ADC-IRQ das neue Bremsbit nicht vor der zugehoerigen physischen Coast-Abschaltung sehen und einen normalen Bremsvorgang irrtuemlich zum PWM-Fault machen. Die physische Timing-Grenze ist dennoch nur am ESC messbar.
- Neue Unit-Tests und kompilierte `main.c`-Integrationsfaelle decken Timeout genau an der Grenze, Neutral-Verfall bei Coast, Gas nach Ausfall, erneuten Neutral-Rollstart und terminales Timeout im angetriebenen Zustand ab.
- **Exakt auf diesem Source-Commit bestanden:** [Drive/ARM CI 39629916521](https://github.com/ZachWay9bot/DeltaESC-G30D/actions/runs/39629916521), [Source-Audit 39629916571](https://github.com/ZachWay9bot/DeltaESC-G30D/actions/runs/39629916571), [UART/IAP-Sicherheitsaudit 39629916747](https://github.com/ZachWay9bot/DeltaESC-G30D/actions/runs/39629916747).
- **Unveraendert:** `iap_update.c/h`, `iap_proto.c/h`, `iap_control.c/h`, `ninebot_link.c/h`, `stm32f103_min.h`, `linker.ld`; keine Loader-, Staging-, Recovery-, CRC- oder Versions-Sperre, keine Stromlimit-Anhebung. Vergleich weiterhin gegen die v0.10.2-DeltaESC-Referenz, kein FO76-BIN-Rueckflash-Beweis.
- **Keine Hardware- oder Fahrfreigabe**: exakte G30-Motorwerte, physische Strom-/Buskalibrierung, Richtung/Phasenbezug und autonom erreichbarer IAP-Rescue bei App-Ausfall fehlen. Die 50-%-Akkuanzeige bleibt Platzhalter.

## Software-Ergebnis
- Stock-Dashboard G30 `0x65` liefert Gas/Bremse, Brake hat Vorrang. Nur ein tatsaechlich akzeptierter ADC-Steuerframe erneuert den Kontroll-Zeitstempel; `0x64` Status-/Heartbeat-Anfragen nicht.
- Sensorloser Start nach passiver BEMF-Erkennung ab konfigurierter Schwelle (aktuell 3,0 km/h) und frischer Null-Gas-Bedingung. Es gibt keinen autonomen Anfahrversuch aus Stillstand.
- Gas proportional innerhalb des vorhandenen niedrigen RC-Stromlimits. Bremse bedeutet **Coast**, keine negative oder regenerative Bremsleistung.
- Normaler Stop durch Gas-null/Bremse ist nicht terminal. Nach Bremse oder verlorenem Steuerframe muss ein neuer gueltiger Null-Gas-Frame beobachtet werden; erst danach darf erneuter Gasbefehl bei genuegender Rollgeschwindigkeit starten.
- Bleibt Gas beim Loslassen der Bremse gedrueckt, startet der Motor nicht automatisch. Das gilt auch wenn vor dem Bremsen bereits neutral freigegeben war.
- ADC-/PWM-/Observer-/Gatefehler, E6 und echte Verbindungs-Timeouts bleiben terminal. IAP hat weiter Vorrang vor motorischer Aktivierung.

## Echte, erfolgreiche Softwarepruefungen fuer den exakten Quellcode
- [Drive-ARM/Native CI 39586098824](https://github.com/ZachWay9bot/DeltaESC-G30D/actions/runs/39586098824): PASS; C-Komponenten, echte `main.c`-Integration und ARM-Build.
- [Zweiter Drive CI 39586091049](https://github.com/ZachWay9bot/DeltaESC-G30D/actions/runs/39586091049): PASS.
- [Quell-/Referenzaudit 39586098867](https://github.com/ZachWay9bot/DeltaESC-G30D/actions/runs/39586098867): PASS.
- [UART/IAP-Sicherheitsaudit 39586098839](https://github.com/ZachWay9bot/DeltaESC-G30D/actions/runs/39586098839): PASS.
- Main-Integration prueft reales Target-`main.c` mit simulierten Registerspeicherzugriffen und inerten ARM-Barrieren; das ist **kein** Motor-, ADC-, Flash- oder Recovery-Hardwaretest.

## Neue Quellcodekorrekturen gegen den zuvor erfolgreichen v0.10.3-Stand
1. `motor_pipeline.c`: Verwaist erhaltene Null-Gas-Berechtigung nach Bremsen oder ungueltiger Steuerung wird zurueckgenommen.
2. `main.c`: Kontroll-Gueltigkeit wird nur durch korrekt dekodierte `0x65`-ADC-Frames erneuert; `0x64`-Statusanforderungen koennen kein altes Gas frisch halten.
3. `drive_main_integration_test.c` / `motor_pipeline_host_test.c`: Brake-Coast/Release/Rearm, Brake waehrend Coast, alte Gasfreigabe und Status-only-Timeout.
4. IAP, Bootloader, Linker, Flash-Geometrie, Recovery/CRC/Identitaetssperren **nicht angefasst**. Es wurden keine produktiven Stromgrenzen angehoben.

## Was **nicht** ehrlich als fahrbereit gelten kann
- **Ziel-G30-Hardware noch nicht mit dieser BIN getestet**: Rolling-Handover unter Last, Sollstrom/Iststrom, Brems-Coast bei realem Rad, Timeout, Schutzabschaltung und nachfolgender Rueckflash fehlen.
- **Kalibriertes G30-Motorprofil fehlt**: R/L/Flux, Phase-/Drehsinn, Polpaare, Radumfang, Strom-/VBUS-Verstaerkung und ADC-/Gate-Timing sind auf dem *Zielmotor* zu bestimmen. FO69/FO76-Donor-E-Bike-Daten sind keine G30-Messdaten.
- `g30_target_profile.h` laesst unbekannte Werte bewusst auf null; die echten Werte koennen RAM-only via F0–F5/FF konfiguriert werden. Ohne reales Profil ist der Kandidat nicht eigenstaendig fahrbereit.
- Stock-Dashboard-Akkuanzeige bleibt 50-%-Platzhalter, bis eine belastbare BMS/SOC-Quelle integriert ist. Aktive Rekuperation ist nicht implementiert und wurde nicht angefordert; Coast bleibt.
- Neue Firmware wurde **nicht geflasht oder freigegeben**. Die physische Rueckkehr zur Stock-Firmware muss am tatsaechlichen Ziel **ohne ST-Link** nachgewiesen sein. VESC-/IAP-Erfolge anderer Images duerfen nicht als diese Freigabe gelten.

## Naechste erforderliche reale Schritte (kein automatischer Hardwareauftrag)
1. Am eintreffenden Controller lesend installierte DRV-Version und erreichbaren bestehenden IAP-Weg zuordnen. Keine neue Bootloader-, Flash-Layout- oder Recovery-Architektur.
2. Bestehenden Rueckweg auf dem *exakten* Ziel vor Firmware-Freigabe mit geeigneter verifizierter Methode nachweisen. Keine irreversiblen Blindflashes.
3. G30-Gen1-Motorprofil inkl. Drehrichtung, ADC-/Gate-Konfiguration und Strom-/Buskalibrierung messen und nachvollziehbar dokumentieren.
4. Dann Wheel-free Versuch mit kontrollierter Versorgung: Anschieben/3-km/h-Rollstart, proportional Gas, Coast, Null-Gas, Wiederanlauf und die bisherigen Fehlerschutzfaelle; BIN-Hash und Logs protokollieren.
5. Erst bei real bestaetigtem Rollback, passender Regelung und Brems-/Abschaltreaktion Release-Hold erneut gemeinsam bewerten.

## Keine unbewiesenen Versprechen
**Stand ist software-gepruefter, funktionsverbesserter RC-Kandidat, nicht fahrfertige oder nachweislich rueckflashbare Release-Firmware.** Das gilt nicht rueckwirkend fuer die real erbrachten FO76-Tests.
