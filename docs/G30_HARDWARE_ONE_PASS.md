# DeltaESC G30D – EINMALIGE Inbetriebnahme nach Lieferung

> Ziel: **keine** Wiederholung von Offline-CI, Mock-Motorlaeufen, X-mal gleichem UART-Preflight oder historischer Fehleranalyse. Die fertig signalisierte Offline-Abnahme ist unser Softwarebeleg. Dieses Blatt enthält ausschliesslich Informationen, die den physischen Controller benoetigen.

## Unverrueckbare Projektgrenzen

- **G30D Gen1** als Ziel; 15 Polpaare sind fuer unsere Gen1-Variante bestaetigt. Umfang, R, L, Flux, elektrischer Phasenbezug und Messverstaerkung sind am neuen ESC noch nicht verifiziert. Kein FO70-Donorprofil in die aktive G30-Fahr-Firmware einschleusen.
- Ohne angeschlossenes Dashboard am Bench: `bench/G30_OnePass_UART.ps1` bildet beim spaeteren Test **ausschliesslich** die normalen G30-`0x65`-Dashboardframes ueber bereits vorhandene UART-Schnittstelle nach. Kein SHU-Build, keine extra Flashschnittstelle.
- Nur bisheriger **IAP**-Pfad; keine erreichbaren SWD/ST-Link-Punkte. **DRV 1.8.7 am 24.10.2026 durch UART READ 0x1A echt bestätigt** (`87 01` / `0x0187`, COM9); Seriennummer bewusst nicht öffentlich archiviert. **DRV126** bleibt geplante Referenzbasis, ist aber NICHT installiert. Die Recovery-/Downgrade-Kompatibilität ist weiterhin **NICHT** belegt.
- Bremse = Coast; keine Rekuperation, kein negatives Motormoment, keine ungefragte Leistungsanhebung. Keine Modifikation an Loader/IAP/Flashlayout, keine zusaetzliche Update-Sperre.
- ACHTUNG: USB-UART nicht direkt mit push-pull-TX auf die single-wire, open-drain-G30-UART-Leitung legen. Physikalisch passende Halbduplex-/Pegelbeschaltung ist Voraussetzung. RemoteLab/andere COM-Besitzer vorher beenden.

## Was NICHT nochmal ausgefuehrt werden muss

Die CI-Tests fuer G30-Gassteuerung, Nullgas, Brake-Coast, Restart, Dashboard-Timeout, ADC/PWM-Hostintegration, IAP-Quelldateivergleich und Cortex-M3-Build wurden vor Lieferung erledigt. Im Hardwaretermin **nicht erneut ausfuehren**. Der finale Freeze-CI-Run und die Hashes liegen im Repository und sind verbindliche Nachweise.

## Hardwaretermin: nur 3 Abschnitte, kein endloser Testkatalog

### A. Einmalig Stock-Istzustand lesen (kein Flash)

1. Controller elektrisch pruefen, Rad frei, strombegrenzte Benchversorgung; UART wie vorher physisch korrekt anschliessen.
2. `powershell -ExecutionPolicy Bypass -File .\bench\G30_OnePass_UART.ps1 -Mode DryRun` bestaetigt lokal die vorbereiteten Frames und schreibt **nichts**.
3. `powershell -ExecutionPolicy Bypass -File .\bench\G30_OnePass_UART.ps1 -Mode Identify -Port COMx -Seconds 3` gibt **einmal** die vorhandene, read-only Identitaetsanfrage aus und speichert die Rohantwort mit SHA256. `COMx` ist der tatsaechliche neue Port, nicht blind COM9.
4. **Erst wenn die Firmware tatsaechlich DeltaESC meldet**, eine einmalige READ-ONLY-Sammelabfrage durchfuehren: `powershell -ExecutionPolicy Bypass -File .\\bench\\G30_OnePass_UART.ps1 -Mode Snapshot -Port COMx -Seconds 4`. Das ruft D0/D1/D2/D3/D4/D5/D7/D9/E8/EB/FC/FD/FE/FF in einem Durchlauf ab. Auf einer fremden Stock-Firmware diese spezifischen App-Abfragen nicht als gueltige Diagnostik interpretieren. Das Skript fuehrt weder Motorstart noch Flash/Erase/Reset aus. Die RX-Bytes werden roh aufgezeichnet. **Einmal danach offline auswerten**: `python bench/G30_Decode_Log.py --log G30_BENCH_YYYYMMDD_HHMMSS.log --output G30_EINMALSTATUS.json`. Die Rueckmeldungen enthalten die Fehlerursache, Blocker, Motorprofil, ADC-Schnappschuss, Rollrichtung und FOC-Zustand, soweit tatsaechlich vom Controller beantwortet. Fehlen Felder oder Antworten, werden sie NICHT als bestanden markiert.
5. Falls stock nicht antwortet, **nicht** mit irgendeinem naechstbesten Flash- oder Recovery-Experiment fortfahren. Das Rohlog genuegt fuer die Zuordnung. Kein extra Dauertest.
6. Kontrollieren, ob existierender IAP-Weg von diesem *tatsaechlichen* Stockzustand aus akzeptiert wird und der bekannte Rueckweg real moeglich ist. Ein Softwaretest kann die fehlende bootloader-unabhaengige UART-Recovery nicht ersetzen. Deshalb vor Bilduebertragung kein ungeprueftes Upgrade.

### B. G30-Hardwarekennwerte EINMAL erfassen

Erst wenn die *tatsaechliche* Firmware und UART-Geometrie zugeordnet sind, die G30-spezifischen Werte einmal messen/erfassen: Phasendrehsinn und BEMF-Reihenfolge; R/L/Flux bzw. passende validierte Beobachterparameter; Phasenoffset; ADC-Offsets, ADC1/ADC2-Verstaerkung und Polung; VBUS-Skalierung; Timer/ADC-Samplingfenster; Radumfang. Pole pairs fuer unsere Gen1-Variante = **15** (bisherige Bestaetigung), ansonsten nicht alle unbekannten Werte einfach mit fremden Messwerten auffuellen.

Die folgenden `F0 F1 F2 F3 F4 F5 FF`-Konfigurationsframes koennen **offline, erst aus den gemessenen Zahlen** mit `bench/G30_Config_Packets.py` zusammengestellt werden. Das Tool oeffnet **keinen** Port, uebertraegt **nichts**, arbeitet nur mit RAM-Konfiguration der passenden DeltaESC-App. Es darf nicht mit DRV 1.8.7 / DRV126 verwechselt werden. Aktuell werden bewusst keine falschen Defaultwerte eingesetzt.

### C. Ein einziger klar definierter Wheel-free-Motorlauf

Erst nach kompatibel nachgewiesenem IAP-Flash-/Rueckweg und korrektem Motorprofil: mit strombegrenzter PSU und frei stehendem Rad ADC-/Gate-Messwerte lesen und den bereits getesteten `ROLL -> ROLL_VERIFY -> CLOSED`-Pfad mit kleinem, ueberwachten Stromkommando pruefen. Der Bench hat kein Dash; spaeter koennen echte `0x65`-Frames ueber den vorhandenen UART gesendet werden:

- `-Mode Neutral -Port COMx -Seconds 3` sendet nur Nullgas-Frames.
- `-Mode Pulse -Port COMx -Throttle 35 -Seconds 3 -WheelFreeConfirmed -CurrentLimitedPSUConfirmed -FirmwareIdentityConfirmed` sendet erst Nullgas, dann einen kleinen begrenzten Gasimpuls, dann Nullgas. Der Benutzer startet es **nur** nach der physischen Verifikation; niemals automatisch. Ein manuell gedrehtes Rad oberhalb der konfigurierten Rollschwelle ist Voraussetzung.
- Drei (!) Schalter im PowerShell-Tool sind **nur lokale Bedienbestätigungen** fuer einen aktiven Test und veraendern weder Firmware noch Bootloader.

Einmal pro realem Motorlauf nur das Log `0xFD/0xFE`, Iststrom-/ADC-Rohwerte, Statefolge, und IAP-Erreichbarkeit nach dem Lauf sichern. Die `Snapshot`-Funktion erfasst diese Felder in einer Sitzung, und der Decoder erzeugt anschliessend ein reproduzierbares JSON ohne erneute Hardwareabfrage. Nach Bedarf gezielt **einen** Fehler beheben, statt das ganze Projekt wieder zu testen.

## Was ohne reale Hardware prinzipiell NICHT bestaetigt werden kann

- Tatsächliche Flash-/IAP-Kompatibilitaet und anschliessende Rueckkehr zu DRV126 auf dem eintreffenden Controller.
- G30-Motor-/Analogwerte, Phasen-/Stromvorzeichen, ADC-Apertur, Gate-Hardware und MOSFET-Thermik unter Leistung.
- Realer sensorloser FOC-Handover bei 3 km/h, Last/Gas, reales Coast und anschliessendes korrektes Wiederanlaufen.

**Keine Strassenfreigabe durch CI.** Ein erfolgreicher Hosttest ist ein Softwarenachweis. Ein funktionierender echter FET-Brueckenbetrieb benoetigt genau diese Hardwarepruefungen, nicht noch 200 frei erfundene Preflights.

## Abnahmekriterien (einmalig)

- Originalcontroller-Identitaet und bestehender IAP-Rueckweg sind nachvollziehbar dokumentiert.
- Gemessenes G30-Profil passt zu Polpaaren / Dreh-/Phasenfolge / ADC / VBUS.
- Rad dreht kontrolliert, Gas ist proportional, Bremse und Nullgas lassen einfach ausrollen, neuer Neutral->Gas-Zyklus funktioniert.
- Fault-Abschaltung arbeitet, nach Rueckkehr zu Neutral liegt keine unerwartete Gateansteuerung an.
- IAP-Zugang besteht nach dem Test weiter, Stock-Rueckweg ist tatsaechlich belegt.

Ab dann keine weiteren ritualisierten Wiederholungstests ohne konkrete Regression.

## Hardware-Befund 24.10.2026: DRV187 vorgefunden

**ECHT GEMESSEN**, G30D-Controller mit Gen1-Motor angeschlossen: Original-Ninebot-Identitätsantwort auf READ 0x10; Versionsregister READ 0x1A antwortet `87 01`, also DRV **1.8.7 (0x0187)**; COM9/115200 und Halbduplex-READ funktionieren. Keine Flash-/Motoraktion ausgeführt, keine private Seriennummer im öffentlichen Repository. Der alte Plan "DRV126 auf Stock als gegeben annehmen" ist damit ausdrücklich widerlegt.

**RELEASE-HOLD**: Die offizielle DRV187 hat laut externer ScooterHacking-Dokumentation Downgrade-Schutz; historische Werkzeuge versagen typischerweise. Trotz einzelner aktueller Community-Anleitungen (2026) ist für **diesen konkreten Controller** weder eine unverändert erhaltene UART-Recovery noch ein passender DRV126-/DeltaESC-Upload belegt. Kein IAP-BEGIN (0x07), ERASE, CRC, RESET und kein SHU-Flashversuch auf Verdacht. Ein erfolgreicher READ beweist nur UART-App-Kommunikation, **nicht** IAP- oder Bootloader-Recovery. Bei fehlender unabhängiger Recovery-Möglichkeit das bestehende DRV187 belassen und kompatiblen Rückflashpfad zuerst nachweisen. Die DeltaESC-DRIVE-BIN bleibt SOFTWARE ONLY / NICHT FLASHEN.

Quelle zum bekannten DRV187-Flashschutz: https://joeybabcock.me/wiki/Ninebot_Max_G30_DRV_1.8.7 und https://joeybabcock.me/wiki/Ninebot_Max_G30/G2_Update_Failed_-_Data_Format_Error. Neuere gegenteilige Community-Aussagen nicht automatisch als Hardwarequalifikation ansehen.
