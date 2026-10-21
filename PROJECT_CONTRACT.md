# PROJECT_CONTRACT — DeltaESC

Der gemeinsam freigegebene Entwicklungsvertrag gilt fuer dieses Firmwareprojekt:

**[IntheLoopRemoteLab / PROJECT_CONTRACT.md](https://github.com/ZachWay9bot/IntheLoopRemoteLab/blob/main/PROJECT_CONTRACT.md), v1.0 vom 20.10.2026.**

Die verbindliche versionsuebergreifende Schichtuebergabe liegt unter [IntheLoopRemoteLab / PROJECT_HANDOFF.md](https://github.com/ZachWay9bot/IntheLoopRemoteLab/blob/main/PROJECT_HANDOFF.md).

Ziel: **bekannte G30D Gen1-Hardware**, kein universelles Multi-Controller-Release. Bestehende funktionierende Flash-/IAP-/Recovery-Wege und Motor-Schutzfunktionen nicht ohne konkrete Freigabe veraendern. Die aktuell dokumentierten Release-Holds werden durch diesen Link nicht aufgehoben oder umgeschrieben.

Fuer Offlinearbeiten: nur Source, Build, Simulation und Tests, ohne echten Controllerbetrieb.

Ein neuer Chat muss vor Aenderungen Vertrag, aktuelle Uebergabe, diesen Repo-Stand und Git-Commit pruefen. Projektentscheidungen sind kein Anlass fuer ungefragte Kompatibilitaets- oder Bootloader-Sperren.

## Explizite DeltaESC-Freigabe vom 21.10.2026

Der Projektbesitzer hat die zuvor zu weit ausgelegte Zustimmungspflicht
korrigiert: Sie betrifft fuer diesen Auftrag den Rueckflashpfad. Motorfreigabe
und die fuer den Fahrbetrieb notwendigen Aenderungen sind erteilt. Innerhalb
dieses Auftrags sind keine erneuten Einzelgenehmigungen fuer Motorstart,
Regelung, normales Ausrollen/Wiederanlaufen oder Schutzlogik erforderlich.

Prioritaet hat immer die Rueckkehr zur vorherigen Firmware **ohne ST-Link**.
ST-Link ist am Zielcontroller **nicht verfuegbar**; alte Hinweise darauf sind
keine aktuelle Recovery-Moeglichkeit. Keine zusaetzliche CRC-, Flashgroessen-,
Versions-, Motorfehler- oder Kalibrierungssperre darf den vorhandenen
Rueckflashweg blockieren. Bestehende Leitungen/Kommandos, Staging und
Bootloader bleiben Referenz. Nicht beweisbar reversible Hardwareaktionen
werden durch diese Freigabe nicht zu nachgewiesen reversiblen Aktionen.

Source-, Build- und Hosttestarbeit wird selbststaendig fortgesetzt. Die
physische Erreichbarkeit des Rueckflashs ist vor einer Flash-Empfehlung am
exakten Ziel nachzuweisen; ein in der App liegender Handler allein ist kein
Rescue-Nachweis fuer eine nicht startende App. Diese Praezisierung gilt fuer
den aktuellen DeltaESC-Auftrag und ersetzt die widersprechenden alten
Motor-Freigabeannahmen in den Uebergaben.
