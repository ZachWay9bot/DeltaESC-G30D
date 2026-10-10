# DeltaESC Motor-RC – passiver BEMF-Anschiebetest (Voraussetzungsprüfung)

**11.10.2026. Kein Firmware-Flash und keine Motorfreigabe für diesen Test.**

Ziel: Vor einem neuen DeltaESC-Build herausfinden, ob der originale,
vergossene G30D-Gen1-Controller überhaupt **drei getrennte
Phasenspannungen** an geeigneten ADC-Pins misst. Ohne diese Quellen kann
aus einer reinen Schubfahrt bei Gate-OFF kein belastbarer elektrischer
Rotorwinkel aus den vorhandenen Stromshunts gewonnen werden.

## Was wir geprüft haben

- Der per DRV126 nachgewiesene ADC1/2-Pfad auf **PA3/PA4/PA5**
  erfasst sektorbezogene Motorströme (Shunt-Spannungen, Nullwert ~2048).
- Der ebenfalls belegte **PA1**-Pfad erfasst die Zwischenkreisspannung.
- **PB1** ist in älteren DeltaESC-Dokumenten als **BEMF_C**, in
  https://github.com/Vicy-DE/Ninebot-G30-Custom/blob/main/boards/esc-motor/PINOUT.md
  aber als motorbezogener GPIO-Ausgang gekennzeichnet. Andere wichtige
  Pinannahmen dieses externen Dokuments weichen von unseren G30D-3-Cap-
  Befunden ab. Beide sind deshalb für den hier geforderten
  BEMF-Phasenbeweis **nicht autoritativ**.
- Selbst ein korrekt belegtes einzelnes BEMF_C wäre keine vollständig
  vermessene Dreiphasen-Phasenspannungs-Schnittstelle.
- Ohne nachgewiesene Phase-U/V/W-Sense-Pfade werden weder BEMF-Winkel
  noch Observer-Lock, Drehzahl oder 3-km/h-Freigabe vorgetäuscht.
- Das nicht motorfähige **v0.9.18** bleibt unangetastet; die bereits
  funktionierende Android-App wird nicht verändert.

## Was jetzt konkret ohne Endstufe möglich ist

Das Skript `tools/G30D_Passive_BEMF_ADC_Audit.ps1` erfasst
ausschließlich **Konfigurationsregister**, nicht die ADC-Datenregister
(DR/JDR) und schon gar nicht Motor-Output. Der zweite Schritt
`tools/parse_swd_adc.py` dekodiert die aktuell durch SHFW/DRV
selektierten ADC-Kanäle. So können wir den nächsten Schritt
hardwarebezogen entscheiden, statt ADC3-Phasenspannungen zu erfinden.

Für Windows PowerShell aus dem heruntergeladenen `motor-rc/tools`-Ordner:

```powershell
powershell -NoProfile -File .\G30D_Passive_BEMF_ADC_Audit.ps1
python .\parse_swd_adc.py .\G30D_BEMF_AdcMap_<Zeitstempel>.txt
```

Falls `STM32_Programmer_CLI.exe` nicht im PATH ist, den bekannten
Installationspfad mit `-Cli "C:\...\STM32_Programmer_CLI.exe"` übergeben.
Vor Ausführung nur 10S, Scooter gegen unerwartete Bewegung sichern,
kein Gas/Bremse, kein Laden, keine Änderung von Phasenkabeln unter
Spannung. Das Skript nutzt ausschließlich
`-c port=SWD mode=HOTPLUG freq=1000 -r32 <addr> 0x4`.
Eine SWD-Verbindung ist trotzdem ein Eingriff in das laufende System,
darum bei Verbindungsfehlern, Reset oder Bewegung abbrechen.

**Achtung:** Die aktuelle Firmware kann andere ADC-Kanäle verwenden als
DRV126; aus nicht gescannten Kanälen folgt noch **kein** physikalischer
Beweis für das Fehlen einer Phasenspannungsschaltung. Umgekehrt beweisen
ADC-Ranks oder GPIO-Analogmodi **keine** Leiterbahnverbindung zum Motor.
Erst eine gesicherte Schaltungszuordnung erlaubt eine echte,
gate-freie BEMF-Winkelmessung.

## Alternative Konsequenz

Sollten drei nutzbare Phasenspannungs-Eingänge fehlen, ist genau der
vorgeschlagene **passive Anschiebe-Observer ohne Leistungsschaltung
nicht implementierbar**. Der VESC-Ortega-Observer kann mit gültig
erfassten Strom/Spannungswerten bei aktiver Anregung arbeiten; er kann
die Rotorposition bei ausgeschalteten Gates nicht allein aus den
Ruhe-Shunt-ADCs ableiten. Dann planen wir keine wertlose weitere
Firmware/App, sondern nutzen eine tatsächlich geeignete
Phasenspannungs-Messquelle oder ändern den Testansatz.

## Entwicklung

- `tools/test_parse_swd_adc.py` prüft 1/2/3 ADC-Ranks, STM32F103
  JSQR-Rechtsbündigkeit, GPIO-Analogbits, Fehlerfälle und CLI-Parser.
- [CI für die Messvoraussetzung](../../.github/workflows/motor-rc-passive-bemf-adc-audit.yml)
  führt die Tests durch und prüft das reine READ-Skript.
- Der Branch enthält kein neues Motor- oder BEMF-Flashimage und erteilt
  ausdrücklich **keine Flashfreigabe**.
