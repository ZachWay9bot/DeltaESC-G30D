# DeltaESC G30D — Fortschritts-Freeze vom 09.10.2026

**Projektcheckpoint; keine Firmwareänderung und keine Flash-Freigabe.**

## Eingefrorene technische Basis
- Firmware: **DeltaESC v0.8.12**, D0-Build-ID **0x080C**
- Quellcode-Freeze: `freeze/deltaesc-g30d-v0.8.12-offset-noise-quality-gateoff-ci-green-2026-10-09`
- Commit: `60d6c4d6243a8693101dc7ce22899baef5509f02`
- CI: https://github.com/ZachWay9bot/DeltaESC-G30D/actions/runs/37877235113 (**green**, Quellcodepaket, keine Flash-Firmware)
- G30D Gen1 Original-ESC bleibt auf Stock DRV126; DeltaESC noch **nie mit aktivierter Leistungsstufe physisch validiert**.
- Sensorless-Regelkern softwaregetestet. Feste ADC1/ADC2-Zweirangdiagnose, 512er Offset-Kalibrierung mit CH5-Duplikat- und Rauschverwerfungsgrenzen, read-only F6. FOC-Motorbetrieb weiter blockiert.
- Motor-Gates hart per Compile-Guards **OFF**. Kein Blindflash, kein fahrfertiges Image.
- Auf dem Smartphone ist BLE-Verbindung von DashBLE auf Stock-Firmware bestätigt. DashBLE ADC Safety v0.3.4 ist **nicht** für Firmware-Build 0x080C freigegeben. Konfiguration und Diagnose bleiben geplantes BLE/Handy-Interface, ST-Link nur für Recovery.
- ReFlasher/ST-Link-Wiederherstellung ist grundsätzlich plausibel, aber **nicht auf genau diesem Controller mit einem vollständigen Stock-Rollback erfolgreich erprobt**. RDP-Unlock kann Mass-Erase auslösen. Die Recovery-Route ist damit NICHT als garantiert zu behandeln.

## Bewerteter Fortschritt (subjektive Planung, nicht Testquote)
- **Gesamt bis fahrfähiger Sensorless-Firmware: 40 %**
- **Software-Grundlage: ca. 80 %**
- **Erster kontrollierter Motorlauf: ca. 50 % Vorbereitungsgrad**, noch **0 tatsächliche Motorlauf-Tests**
- **Reale Hardwarevalidierung: 0 %** für DeltaESC auf der Leistungsstufe

## Offene technische Gates (nicht durch CI ersetzbar)
1. **Recovery**: STM32-Identität, Option-Bytes und SWD mit ST-Link nichtdestruktiv verifizieren; Stock DRV126 / ReFlasher-Rollback zunächst am Ersatzcontroller vollständig testen.
2. **Strommessung**: ADC-Zeitfenster, simultane Rangmessung, Strom-Shunt-Verstärkung/Polarität, CH3/4/5-Zuordnung und Kalibriergrenzen physisch validieren.
3. **Motor-Ansteuerung**: TIM1 komplementäre PWM-Kanäle, Gate-Polaritäten, Totzeit, Überstrom-Hardwareabschaltung und sichere Boot-Zustände messen.
4. **Sensorless-Motorlauf**: Spannung/Strom begrenzen, erster Test mit freiem Rad und separater Not-Aus-Möglichkeit, Motorparameter R/L/Flux messen; Gas loslassen = Motor AUS, Bremse ohne Motorbetrieb.
5. **Fahrbetrieb**: 10S-Busspannungs-Skalierung und Lastfälle validieren. 14S sowie Stern/Delta-Umschaltung erst nach separater Qualifikation.

### Freeze-Bedeutung
Dieser Stand bleibt unverändert und dient als **Baseline für v0.8.13 oder folgende Hardware-Validierung**. Weiterentwicklungen erfolgen auf neuem Dev-Branch; der eingefrorene Quellcodezweig bleibt unverändert.
