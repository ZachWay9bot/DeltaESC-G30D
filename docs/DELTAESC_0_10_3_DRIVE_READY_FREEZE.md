# DeltaEsc 0.10.3 Drive Ready

**FREEZE** | 21.10.2026 | Nutzerfreigabe: Softwarestand einfrieren, jetzt ausschliesslich EBiCS und SmartESC weiterentwickeln.

## Eingefrorener Bezug

- Projekt: `ZachWay9bot/DeltaESC-G30D`
- Fest benannter Freeze-Branch: `freeze/deltaesc-0.10.3-drive-ready`
- Eingefrorener **Motor-/Firmware-Source-Commit**: `26629227ad7cc882fb3f68426153c83767ce5e3e`.
- Quellinhalt bei Freeze vom Entwicklungszweig `dev/drive-reversible-20261021`, letzter Dokumentationsstand vor Freeze: `3f74686b9fbf9455bdea25ab2ca2eed7a1bd65bb`.
- Firmware-Version im Source: v0.10.3, `FW_BUILD=0x1030`.
- Offene historische Entwicklungs-PR: [#21](https://github.com/ZachWay9bot/DeltaESC-G30D/pull/21); **nicht** als Merge- oder Flashfreigabe behandeln.

## Definition des Namens Drive Ready

`Drive Ready` ist der **vom Nutzer gewuenschte Name dieses Software-Freeze**, nicht die Aussage, dass ein echter G30D mit dieser Firmware bereits fahr- oder flashfreigegeben waere.

Softwareumfang am eingefrorenen Commit:
- G30-Stock-Dashboard 0x65: Gas proportional innerhalb unveraenderter, niedriger RC-Stromgrenzen; Bremse hat Vorrang.
- Bremse/Gas-null: Hardware-Coast, **keine** Rekuperation. Nach Bremsen erst neuer Gas-null-Frame, dann neuer Gasbefehl bei neuer BEMF-/Rollstartqualifikation um 3 km/h. Kein unerwarteter Wiederanlauf mit gehaltenem Gas.
- Veraltete Gas-Nullstellung bei Timeout im Coast-Zustand verfaellt. Dashboard 0x64 Status/Heartbeat erneuert **nicht** die Frische der 0x65-Gasdaten. Bei Timeout waehrend bestromtem Motor terminaler Stopp.
- Atomare Verarbeitung von Gas/Bremse und Motor-Coast gegen ISR-Rennen. Fehlerhafte ADC-, Observer-, Gate- und IAP-Zustaende blockieren Motorstart; E6/Fehler bleiben terminal.
- Originale Low-Current-FOC-/PWM- und Schutzgrenzen nicht angehoben.

Exakte Softwarechecks:
- [Native + ARM Drive CI #39629916521](https://github.com/ZachWay9bot/DeltaESC-G30D/actions/runs/39629916521) **SUCCESS**, `head_sha=26629227ad7cc882fb3f68426153c83767ce5e3e`.
- [Source-Audit #39629916571](https://github.com/ZachWay9bot/DeltaESC-G30D/actions/runs/39629916571) **SUCCESS**, selbe SHA.
- [UART/IAP-Anti-Lockout-Audit #39629916747](https://github.com/ZachWay9bot/DeltaESC-G30D/actions/runs/39629916747) **SUCCESS**, selbe SHA.

## Absolute Sperre fuer weitere Entwicklung

- **DeltaESC wird ab sofort nicht mehr weiterentwickelt, optimiert, refaktoriert, mit anderem Motorcode synchronisiert, gemergt oder experimentell erweitert.**
- Weder Freeze-Branch noch produktive DeltaESC-Dateien ohne **neue ausdrueckliche Nutzerfreigabe** aendern. Ein anderer Auftrag fuer EBiCS/SmartESC schliesst DeltaESC-Aenderungen nicht ein.
- Bestehende funktionierende IAP-/UART-/Bootloader-/Recovery-/Staging-/Linkerwege nicht anfassen. **ST-Link ist physisch nicht moeglich.**
- Die erfolgreichen realen FO76-Flash- und Motorbench-Tests bleiben historisch bestaetigt, aber sind kein Hardwarebeleg fuer diesen v0.10.3-Kandidaten. FO91/0.9.1 fuehrte zu einer Rueckflashregression und darf nicht wieder als Basis in Rueckflashlogik einfliessen.
- Kein Flashen, keine neue BIN als `flash ready`, kein Release-Merge, kein Hardwaremotorlauf ohne getrennte Freigabe und gesicherten realen IAP-Rueckweg.
- Bis zu spaeterem explizitem Entfrieren: **Projektstatus SOFTWARE-FREEZE / HARDWARE-RELEASE-HOLD**.

Offene externe Hardwarequalifikation (nicht Gegenstand aktiver Softwareentwicklung): G30-spezifische R/L/Flux/Phasenrichtung/Polpaare, ADC-/VBUS-Kalibrierung, Wheel-free/Last-/Bremstest auf exakt dem Zielcontroller sowie dessen realer Rueckflash ohne ST-Link. Akku-Prozentwert derzeit Platzhalter.

## Arbeitsfokus ab diesem Freeze

**Nur noch EBiCS und SmartESC**. Zentrale Aufgaben und Referenzen im [IntheLoopRemoteLab](https://github.com/ZachWay9bot/IntheLoopRemoteLab); fuer diese beiden Projekte nur bestehende funktionierende Flash-/Recoverywege erhalten, neue Software strikt getrennt behandeln.
