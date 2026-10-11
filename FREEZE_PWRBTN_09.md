# Freeze „pwrbtn - 09“ — DeltaESC G30D

**Datum:** 11.10.2026  
**Freeze-Name:** `pwrbtn - 09`  
**Codebasis:** `dev/v0.9.18-adc-proof-ble-motorbench`  
**Freeze-Branch:** `freeze/pwrbtn-09-2026-10-11`  
**Status:** SOURCE ONLY · KEINE MOTORFREIGABE · NICHT FLASHEN · KEIN FAHRBETRIEB

Dieser Branch ist eine unveränderte Kopie des aktuellen v0.9.18-Entwicklungsstandes, ergänzt ausschließlich um dieses Freeze-Protokoll. Er überschreibt weder die bisherigen Freezes noch die Entwicklung.

## Gesicherte Originalhardware

- Ninebot G30D Gen1 mit originalem vergossenem **STM32F103 3-Cap-ESC**, drei MT8006A-Gate-Treibern und verstärkten MOSFETs/Kondensatoren.
- Kein ADC-Mod. Der Roller läuft nach Nutzerangabe mit Original-/Stock-kompatibler Firmware und Stern/Dreieck-Umschaltung bei 60 V; das ist **keine** Freigabe für DeltaESC bei 60 V.
- ST-Link/SWD und Oszilloskop vorhanden, **kein strombegrenztes Labornetzteil**; SWD-Zugriff im Originalzustand belegt.
- Als Referenzen stehen der gezeigte G30D-Schaltplan und der originale DRV126-Full-Dump zur Verfügung. Keine weitere Öffnung des vergossenen ESC erforderlich.

## PWRBTN / GPIO — nicht erneut erraten

- Der bereitgestellte Schaltplan bezeichnet **PC14 / physischer STM32-Pin 3** als `PWR_BTN`, **PA11** als `PWR_EN`, **PB1** als `BEMF_C` und **PB12** als `AMP_LIM`. Das ist die dokumentierte Verdrahtung im Schaltplan, nicht in jedem Punkt ein elektrisch bestätigter Signalverlauf des vergossenen Einzelcontrollers.
- **Echte SWD-Idle- und gedrückte-Taste-Messung (10.10.2026):** PA12 in `GPIOA_IDR=0x0000D804` blieb HIGH; PC14 in `GPIOC_IDR=0x00000000` blieb LOW; bei beiden wurde **keine** Pegeländerung aufgezeichnet. Die direkte Power-Button-Zuordnung am realen Controller ist **NICHT VERIFIZIERT**. Die v0.9.0-PA12- und v0.9.1-PC14-Tastenannahmen dürfen nicht als Hardwarebeweis ausgegeben werden.
- Der grüne Dashboard-Power-Tasterdraht wird auf den schwarzen GND-Draht geschaltet. Der Weg über eine vorgelagerte Versorgungshalteschaltung ist plausibel, am Einzelcontroller jedoch nicht vollständig vermessen.
- **PA11** war im SWD-Idle GPIO-Ausgang HIGH; **PB1** war als Analogeingang konfiguriert, also **kein bestätigter Gate-Enable**.
- **PB12/AMP_LIM** war Idle-Eingang HIGH. Der Schaltplan zeigt einen möglichen active-low-Überstrompfad mit Transistor; Schwellwert, Echtzeitreaktion und unabhängige Gate-Abschaltung sind nicht bewiesen.

## TIM1 / ADC — echter Stock-Idle vs. Hardwarefreigabe

- Stock Idle `TIM1_CCER=0x1000`: CH4 für ADC-Trigger aktiv, sechs Motor-Leistungsausgänge ausgeschaltet.
- Stock Idle `TIM1_BDTR=0xCD40`: MOE=1, AOE=1, BKE=0, BKP=0, LOCK=1, DTG=0x40. **Kein aktivierter TIM1-Hardware-Break in diesem Snapshot**. Insbesondere ist dieses Idle-BDTR-Wort **kein** Muster für eine sichere aktive Motorfirmware.
- Im Stock-Idle wurde ADC1 injected CH4/CH5 beobachtet, ADC2 war aus; dies beweist **nicht** das Strommessfenster während der aktiven DRV126-FOC-Regelung.
- Die echte MT8006A-Gate-Polarität am eingegossenen Einzelcontroller, Abschaltung bei Brake/FAULT/Watchdog, analoger AMP_LIM-Schutz, nominelle DRV126-Stromskalierung in Ampere, shunt-synchrone ADC-Abtastung und 10S/60-V-Buskalibrierung bleiben **hardwareseitig offen**.

## Firmwarestand v0.9.18

- VESC/EBiCS-inspirierter FOC/Observer sowie v0.9.14 R/L/Flux-Motor-Detect-Algorithmus und TIM1-/ADC-Softwareadapter sind in der Quellhistorie vorhanden, **kein realer Motorlauf nachgewiesen**.
- v0.9.16: Guard für BDTR/CCER, verhindert gefährliche AOE/LOCK-Kopien und unkontrollierte Freigaben.
- v0.9.17: verriegelter, terminaler TIM1-MOE-/CCER-Stopp und passive GPIO-Pulldown-Sicherung im Code; **nur Register-/Hostsimulation, keine elektrisch gemessene MT8006A-Abschaltung**.
- v0.9.18: read-only NinebotCrypto-BLE-Register **FA/FB** für ADC-Rohwerte/Offsets, Alter, VBUS, CCER/BDTR, Peak-Rohcount und Fault; `FW_BUILD=0x0918`. Ohne validierte elektrische Stromskalierung bleiben alle Werte ADC-Counts.
- **Motor-Enable, SENSORLESS_RUN und E7-Motor-ID bleiben in v0.9.18 compile-seitig gesperrt.** Die existierende Android-App ist für FA/FB + Build 0x0918 noch nicht entsprechend aktualisiert.
- GitHub-CI v0.9.18 war am 10.10.2026 grün: https://github.com/ZachWay9bot/DeltaESC-G30D/actions/runs/38091719541 . Dies ist Software-Nachweis, kein Flash-/Motorfreigabenachweis.

## Fortsetzung nach dem Freeze

1. **Ohne Nutzer-Flash**: Android-BLE-Motor-Bench-Frontend an v0.9.18 FA/FB read-only anpassen und vorhandenen NinebotCrypto-Transport verwenden. Keine E7-/Motor-Kommandos.
2. **Offline**: verbleibende Hardware-Abschaltpfade sowie ADC-/PWM-Sampling gegen Schaltplan, Original-DRV126 und echte SWD-Logs auditieren; Unklarheiten explizit beibehalten statt zu behaupten, sie seien gelöst.
3. Erst für **Motor-Detect mit echter Bestromung**: elektrisch belegte unabhängige Abschaltung/Überstrombegrenzung, sichere zugängliche Messpunkte, strom-/energiebegrenzter Prüfstand und verifizierter 10S-Betrieb mit freiem Rad. Ein normaler 10S- oder 60-V-Akku erfüllt diese Bedingung nicht alleine.
4. Kein aktives Firmware-Flash, keine Motorfreigabe und kein Versuch, PC14/PA12/BKIN anhand unbelegter Hypothesen zu schalten.

**Kanonische Belege:** `v0.9.15/README.md`, `v0.9.16/README.md`, `v0.9.17/README.md`, `v0.9.18/README.md`, `v0.9.2/DRV126_SWD_BUTTON_FOLLOWUP_2026-10-10.md`.
