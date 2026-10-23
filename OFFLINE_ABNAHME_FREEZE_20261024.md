# DeltaESC G30D – Offline-Abnahme / Freeze (24.10.2026)

**STATUS: SOFTWARE OFFLINE PASS – NEUER CONTROLLER NOCH NICHT VORHANDEN**

## Eindeutige Nachweise

- Arbeitsbranch: `dev/g30-fahrbetrieb-clean-20261023`.
- Original-v0.10.2-SOURCE-Referenz: SHA256 `3a7a4533c743df97f555a199ef603ff52efb34af752301a43ffb0497a3285f6f`.
- Finaler, vollstaendig erfolgreicher CI-Lauf: [40036462792](https://github.com/ZachWay9bot/DeltaESC-G30D/actions/runs/40036462792).
- **Alle 9 fachlichen CI-Schritte und alle Tests PASS**: Original-IAP/Linker-Bytevergleich, PowerShell-Bench-DryRun, Rollrichtungs-/Geschwindigkeits-/DWT-Grenzfaelle, RAM-Parameterpaket-ABI, Dashboard-, Gas-/Coast-/Neutral-/Rearm- und IAP-Parser-Hostintegration, SAFE und DRIVE Cortex-M3, SP/Reset/Flashgroessen-/Symbolpruefung, Paket/SHA256.
- **Aktuelles G30 DRIVE BIN SHA256:** `7b6a17d4df105ae3cf078dccb25ce926bce97fb46902317a812484e9b621bbb1`.
- Paket: [DeltaESC G30 one-time offline freeze (GitHub Actions artifact)](https://github.com/ZachWay9bot/DeltaESC-G30D/actions/runs/40036462792/artifacts/12537435969); Inhalt: unqualifizierte BIN, SAFE BIN, Map, Symboltabelle, Quellarchiv, SHA256SUMS, Hardwareeinmalplan und maschinenlesbares Manifest. Retention 90 Tage ab Erzeugung.
- Die 10 geschuetzten IAP-/USART-/STM32-/Linkerdateien sind bitgenau auf der unveraenderten v0.10.2-Quelle. Im aktiven Zweig keine SHU-Paketwerkzeuge und keine SHU-Releaseartifacts.
- G30-Gen1 Polpaarzahl des Eigentuemers **15**. Physische Messwerte fuer Phasenlage, R/L/Flux, Strom/Spannungsverstaerkung und Radumfang absichtlich nicht vorgetaeuscht.

## Fertige Hilfsmittel fuer exakt einen Hardwaretermin

- `bench/G30_OnePass_UART.ps1`: Default DryRun. Modus `Identify` einmalige lesende UART-Abfrage, `Neutral` emuliert nur null-gasige Dashboardframes, `Pulse` fuer bewussten spaeteren Wheel-free-Versuch mit physisch korrektem Open-Drain-UART-Adapter. Kein IAP-, Flash-, Erase- oder Reset-Befehl.
- `bench/G30_Config_Packets.py`: aus **gemessenen** Motordaten generiert der reine Offline-Paketgenerator die RAM-Konfiguration F0–F5/FF. Kein Senden, keine persistenten Parameter.
- `docs/G30_HARDWARE_ONE_PASS.md`: maximal drei Hardwarephasen (Stock-Iststand, einmalige physische Motor-/Analogkalibrierung, ein kontrollierter Wheel-free-Lauf mit Rueckwegpruefung). Keine wiederholte Host-CI beim Controller-Eintreffen.

## Stoppgrenze des Offline-Freeze

Keine Hardwareeingriffe, kein Flash, kein Stock-Rueckflash und keine reale Motorfahrt ausgeloest. Der eingetroffene Controller koennte DRV 1.8.7 tragen; noch nicht ausgelesen. DRV126 ist geplante Referenz, keine Behauptung ueber die neue Platine. IAP-Zugaenglichkeit bei fehlerhafter Applikation ist nicht nachgewiesen. Kein ST-Link verfuegbar.

**Nach Lieferung zuerst echten Iststand/Rueckweg identifizieren, nur die noch offenen hardwaregebundenen Messungen ein einziges Mal durchfuehren.** Keine weiteren Software-Preflight-Rituale ohne konkrete fehlgeschlagene Funktion oder Codeveraenderung.
