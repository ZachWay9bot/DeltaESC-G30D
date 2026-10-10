# DeltaESC G30D v0.9.7 – ADC / Stromregelung (nur Software)

**Ausschliesslicher Scope:** ADC1/ADC2-Injection, PWM-Abtastfenster, Kanaloffsets, konsistente Strohmess-Skalierung. Freeze v0.9.6 bleibt unveraendert. Keine Aenderung am VESC-Observer, FOC-Algorithmus, Motor Detect, Gate-Backend oder BLE-Protokoll.

## Belegter DRV126-Abgleich

Aus der bereits vorliegenden, hashgebundenen DRV126-Rekonstruktion (`esc126_fulldump.bin` SHA256 `9235b466a2f7449b8184560dbef9012d7c12b099e4076100a223ef68d204bb68`, `DRV126_Sensorless_RE_v1_6_SOURCE_ONLY/reports/adc_sector_v15.json`) stammen folgende Instruktionsanker:

- `0x0800575A`: ADC1 JDR1 (`0x4001243C`).
- `0x0800575C`: ADC2 JDR1 (`0x4001283C`).
- `0x0800590A` und `0x08005916`: ADC-Initialisierungsbeispiele CH3 fuer ADC1 und CH5 fuer ADC2.
- `0x08005738` ff: Sektor aus RAM `0x20000690` wird vor der Stromauswertung gelesen.
- `0x08005716` ff: DRV126 schreibt CCR4 `0x0F9C` in **seinen** Timerwerten. Das ist **nicht** identisch mit DeltaESCs `CCR4=1800`, `ARR=1999`, und kann ohne vollstaendig dekodiertes Stock-TIM1-Timing nicht direkt skaliert werden.

DRV126-Nachweis umfasst damit den synchronen zwei-ADC-Sektorpfad und den alten Triggerwert, **nicht** automatisch ein identisches PWM-Fenster, jede Sektor-JSQR-Umschaltung, die exakte Strompolaritaet oder physikalische mA/count.

## Implementierte Korrekturen

1. **Ein Stromfaktor statt zwei:** `stock_current_nominal_counts_to_ma()` / `stock_current_nominal_ma_to_counts()` liefern einen zentralen, vorlaeufigen Faktor `51575/1024` mA/count. `sensorless_control.c` verwendet exakt dieselbe Funktion. Vorher waren dort 12890/256 und im Frontend 51575/1024 hinterlegt. Die Vereinheitlichung ist mathematisch getestet, ersetzt aber keine Verifikation des Shunt-Verstaerkers.
2. **Fensterpruefung mit RM0008:** ADC1/ADC2 injected simultaneous, JEOC, ADC-Master-/Slave-Trigger, ADC-Prescaler `/6` bei 64 MHz PCLK2 (10,667 MHz), gleiche 13,5-ADC-Takt-Sampling-Zeit fuer CH3/4/5 und 26 ADC-Takte Konversionszeit. Fuer die konkrete DeltaESC-Konfiguration `TIM1 ARR=1999`, `CCR4=1800` verlangt die Software einen konservativen Abstand von 252 TIM1-Ticks zwischen dem fruehesten relevanten Low-Side-Ereignis und der CC4-Probe. Die max. akzeptierten CCR1/2/3 sind damit 1548 fuer beliebige Sektorwechsel. Zu knappes Fenster = ADC-Verwerfungsgrund.
3. **Zweiter ADC hat eigene Offsets:** ADC2 CH4 wird beim Booten mit 256 eigenen softwaregetriggerten, gatefreien Messungen kalibriert, getrennt von ADC1 CH4. Wenn ADC2 CH4 in Sektor 4/5 genutzt wird, gleicht eine gepruefte Offset-Normalisierung die beiden ADC-Pfade ab. Ohne Kalibration oder bei Ueberlauf wird die Probe verworfen.
4. **Fail-closed-FoC:** `main.c` prueft das ADC-Fenster je abgeschlossener IRQ-Probe und sperrt aktuelle Phasenstroeme vor ihrer Uebergabe an FOC. Die TIM1 PWM-Ausgabe weist bereits Software-Kommandos ab, die das konservative Sample-Fenster bei irgendeinem Sektor verletzen.
5. **ARM-Code physisch ins ELF gelinkt:** Die ADC-Checker werden auch im gatefreien SYNC_SAFE-Build per Linker erzwungen; dadurch faellt fehlender ARM-Support bei CI nicht unbemerkt durch LTO/GC heraus.

## Softwaretests

- 21 Hosttest-Gruppen erfolgreich einschliesslich neuer Fenster-, Samplezeit-, Trigger-, Kanal-, Bias- und Vorzeichen-Symmetrie-Tests; UBSan.
- STM32F103 Cortex-M3 Link und BIN erfolgreich, **source-only SAFE**; `COMM_CURRENT_SCALE_HW_VALID=0` und `COMM_ADC_TIMING_HW_VALID=0` bleiben unveraendert.
- Positive Motorfreigabe-Versuche kompilieren absichtlich nicht. Die neue Datei ist **keine** motoraktive Flash-Firmware.

## Grenzen (nicht als erledigt deklarieren)

- Die Stock-Analyse beweist die CH3/CH5-Initialisierungsbeispiele, nicht die exakten ADC-Kanalpaare fuer alle sechs Sektoren. Die DeltaESC-Kanalplanung aus v0.9.6 wird durch die 21 Hosttests nicht zum Stock-Original.
- **ADC-/Shunt-Polaritaet und physikalische Ampere-Skalierung** auf dem individuellen, umgebauten G30D sind mit den vorhandenen Quellen nicht sicher nachgewiesen. Der nominale Faktor ist deshalb keine Freigabe zu einer Ampere-basierten Ueberstromgrenze.
- Das reale ADC-Abtastfenster, Tim1-CC4-Flanke/Bezug und Interrupt-Laufzeit wurden nicht an laufender Leistungsstufe bestaetigt. Software-Windowschutz ist ein plausibles Modell, kein physikalischer Nachweis.

**Ergebnis:** Die identifizierten Softwarefehler sind behoben. Das gesamte ADC-/Stromregelungsthema fuer **realen aktiven Betrieb** ist damit noch **NICHT abgeschlossen**; alles andere zu behaupten waere unzutreffend.