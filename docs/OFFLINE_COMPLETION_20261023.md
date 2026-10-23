# DeltaESC: Offline-Abschluss ohne E-Bremse/Reku — 23.10.2026

## Auftrag und Ergebnisumfang

Der Besitzer hat die weitere Softwarearbeit beauftragt und elektrische
Bremse/Rekuperation ausdruecklich ausgeschlossen. Der Bremseingang nimmt
Motormoment weg und isoliert die sechs Gate-Ausgaenge (Coast).
ST-Link ist physisch ausgeschlossen. IAP ist bis auf Weiteres der Flashweg;
DRV126 bleibt geplante Basis, DRV 1.8.7 der erwartete Lieferstand.

Dieser Nachtrag baut auf dem zuletzt abgeglichenen Branch-Head
`f906e3e97df19029e8b8c05114c403cbda58d13b` auf.
Firmwarekennung der neuen Quelle: **v0.10.3, FW_BUILD 0x1031**.
Native Pruefung: **36 Komponenten-/Integrationsgruppen PASS**,
einschliesslich echter main.c-Entscheidungen und unveraendertem IAP-Parser.
ARM-Build und CI werden am nachfolgenden Quellcommit separat festgehalten.

## Erledigte Software

- Stock-BMS-SOC ueber die separate USART3-Verbindung PB10/PB11,
  115200 8N1, Register 0x32. Nur gueltige READ_ACK-Antworten
  0x22 -> 0x20, exakt zwei Datenbytes, SOC 0..100.
- Akkuwert fuer Stock-Dash und B0 statt festem 50%-Platzhalter.
  Timeout nach 2000 ms; fehlend/veraltet wird als 0 angezeigt,
  B1/BM13 liefert dazu explizite Gueltigkeit und Alter. Ein echter
  0%-Wert bleibt als frisch unterscheidbar. Abfrage alle 500 ms.
- BMS-Treiber mit begrenztem Empfangsbudget, Byte-Ankunftszeitstempeln,
  UART-Fehler-/Overflow-Behandlung und Interrupt-TX ohne Warteschleifen.
  Keine Aktivator-/Schutzschwellen-Kommandos an den Akku.
  IAP wird zuerst bedient; waehrend IAP pausieren BMS-Abfragen.
- Automatischer Start der vorhandenen 512-Sample-Offsetkalibrierung im
  Fahrziel: nach Bootwartezeit und frischen neutralen Dashboard-/ADC-Daten,
  bei 100 ms ruhigem passivem BEMF und nachgewiesen getrennten Gates.
  Kein E0-App-Knopf pro Einschalten. Eingabe/Bewegung/IAP verwirft
  die unvollstaendige automatische Messung. Vorhandene Qualitaetsgrenzen
  und terminale Fehler bleiben. Das ersetzt keine R/L/Flux-Messung.
- Ein einzelner frischer Gas-null-Frame nach normalem Gasbetrieb zaehlt
  jetzt bereits als Neutral. Vorher war unbemerkt ein zweiter Null-Frame
  noetig. Der neue echte-main-Regressionsfall scheiterte am alten Code
  und besteht mit der Korrektur. Bremse oder ungueltige Eingabe braucht
  weiterhin einen anschliessenden, separaten Neutral-Frame.
- Debug-USART1 explizit auf PB6/PB7 remapped. Die Standardroute PA9/PA10
  teilt Pins mit TIM1-Gates und darf nicht aus einem vorherigen
  Firmwarestand uebernommen werden. USART3 setzt seine PB10/PB11-Route;
  beide lassen USART2-Remap und IAP-UART-Konfiguration unveraendert.
- Endlose ADC-RSTCAL/CAL-Warteschleifen durch begrenzte Abfragen ersetzt.
  Ein ADC-Initialisierungsfehler verriegelt die Bruecke und kehrt zur
  weiteren UART/IAP-Initialisierung zurueck.
- Ein haengender Debug-TX wird zeitlich begrenzt erkannt; danach bleibt
  die Debugausgabe stumm, damit die main/IAP-Schleife weiterlaufen kann.
- Saemtliche IAP-Dateien, ninebot_link, CPU-Registerheader und Linker
  sind weiterhin bytegleich zum unveraenderten v0.10.2-Archiv.

## Verifikation und Grenzen

Geprueft sind alle 65.536 Gas-/Brems-Bytekombinationen ohne elektrischen
Bremsbefehl, echte-main-Coast mit getrennten Gatepins, normaler Wiederanlauf,
Bremse/Neutral/Timeout, automatische Offsetmessung mit expliziten Hostdaten,
BMS-Wireformat, alle SOC-/Laengenwerte, Bitfehler, falsche Quelle/Kommandos,
Teilpaket-/SOC-Timeout und Zeitzaehlerueberlauf. BMS-UART-RX-Overflow,
fehlender TX-IRQ, alte gepufferte Daten und IAP-Vorrang sind in der
Register-Speicher-Integration abgedeckt. Defekte Debug-/ADC-Peripherie
kehrt im Hosttest aus dem echten Anwendungscode zurueck.

Die Hostregister und ADC-Werte sind Test-Fixtures; sie emulieren keinen
Motor und belegen keinen elektrischen Betrieb. Auch diese Aenderungen
machen aus einem App-IAP keinen von jeder defekten App unabhaengigen
Rescue-Bootloader. Kein neuer Bootloader und keine neuen Flashseiten
wurden erstellt. Kein Controller geflasht oder Motorjob versendet.

Die BMS-Referenz ist die vorhandene
[SmartESC-G30-BMS-Quelle](https://github.com/ZachWay9bot/SmartESC_STM32_v21b/blob/b4e7bbb38c9738b0f58bdbd1ff28c61c5d0b279e/common_files/src/task_init.c)
mit zugehoeriger USART3-MSP-Pinbelegung. Die Remap-Bits sind durch den
STM32-CMSIS-Header des vorhandenen Projekts abgeglichen.

## Was konkret auf Hardware bleibt

1. Tatsaechenstand des gelieferten Controllers und IAP-Weg auf die
   geplante DRV126-Basis mit dem bekannten Rueckweg abgleichen.
2. Ziel-R/L/Flux, Phasen-/Drehrichtungsframe, Radgeometrie sowie echte
   Strom-/Spannungsskalierung und ADC/Gate-Timing bestimmen.
3. Reale Boot-Offsetmessung, BMS-Antworten und Dash-SOC pruefen.
4. Rolluebernahme, proportionale Gasvorgabe, Gas weg/erneuter Start,
   Coast ueber den Bremseingang und die Fehlerabschaltungen pruefen.
   Danach Last-/Temperatur- und Stromgrenzen anhand realer Daten.
5. **Keine elektrische Bremse/Reku in diesem Auftrag.** Das ist kein
   offener Entwicklungsblock mehr.

Unbekannte Motorprofilwerte bleiben unbekannt; keine Donorwerte werden
als gemessener G30-Parametersatz eingesetzt. Die bisherigen niedrigen
RC-Strom-/Modulationsgrenzen werden nicht fuer Fahrzeugleistung angehoben.
