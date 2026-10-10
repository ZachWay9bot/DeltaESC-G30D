# DeltaESC G30D | Vorbereitung erster 10S-Motortest

**Freeze:** `v0.7.2 MOTOR TEST BENCH`, Build `0x0720`. **Status: Vorbereitung, KEINE Motorfreigabe.** Die Android-App ist `G30 Motor Bench v0.3.3`; ST-Link dient zum Flashen/Recovery, Diag und CFG nur per Smartphone/Stock-G30-BLE mit verifizierter NinebotCrypto-MIC. SHU-Recovery bleibt zurückgestellt.

## 0. Sperren vor echtem E5-Test

- **Firmwareseitige autonome Abschaltung verifizieren/ergänzen.** v0.7.2 weist keinen nachgewiesenen, verbindungsunabhängigen Motor-Test-Timeout von 1000 ms aus. Der APP-STOP nach 1200 ms kann bei BLE-Abbruch nicht garantiert werden. Ein sauberer BLE-Disconnect muss unabhängig zum Motor-OFF führen.
- Boot- und Power-Hold-Leitungen und Polarität am **genau zu flashenden ELF/BIN** noch einmal gegen DRV126/G30-Schaltplan prüfen; PA11 Power-Hold und PC14 Power-Button nicht aus einer früheren Modellannahme ableiten. Zuerst Gate-OFF-Bootprüfung.
- Gate-Ausgänge, TIM1-Break PB12 (active low), reale Strom-Offsets, ADC-Phasenzuordnung PA3/PA4/PA5, Offset-minus-ADC-Vorzeichen und Abschaltpfad ohne Last überprüfen.
- Kein Upload in Bootloader-/Konfigurationsbereiche; STM32-F103/ST-Link-Recovery muss lokal greifbar sein. Auch mit ST-Link kann eine falsche Power-Hold-Implementierung den Controller unbenutzbar erscheinen lassen.
- E5 bleibt gesperrt, wenn eine dieser Bedingungen nicht nachweisbar ist.

## 1. Mechanischer und elektrischer Aufbau

1. Hinterrad **vollständig frei**, Fahrzeug stabil fixieren, niemand im Rad-/Kabelbereich. Keine Fahrt, kein Bodenkontakt, keine Stern/Delta-Relaisumschaltung.
2. Ausschließlich passender **10S-Akku** mit geeigneter Sicherung/Schutzfunktion; keine 14S-Versorgung. Motorphasen sicher verbunden, Stecker, MOSFETs, Shunts, BMS und Kühlung prüfen.
3. Physisch erreichbare Unterbrechung der Motor-/Akkuleistung vorsehen; Telefon oder Bluetooth sind kein Not-Aus. ST-Link-Reflash-Möglichkeit vorbereiten, nicht parallel Motorsteuerbefehle über ST-Link senden.
4. Lüfter/Temperaturüberwachung, Stromaufnahme und Busspannung extern beobachten. Erst spannungsloser Pin- und Durchgangscheck.

## 2. Smartphone-Vorprüfung (ohne E5)

1. App verbinden, G30-Dashboard-Pairing über **5B/5C/5D**, keine MIC-Fehler. Erwartete Stock-ESC-Leseroute `0x20`; bei Stock DRV126 kein Motor-Start.
2. **D0 muss `DESC` plus Build `0x0720`** melden. Jede andere ID sperrt den E5-Knopf. Das ist Absicht.
3. **D1/D2/D9** lesen, Offsets und Faultcodes protokollieren; Phasenmapping und aktueller Gate-Zustand plausibel. Kalibrierung `E0` nur *disarmed*.
4. D0-D9 durchlesen, insbesondere **D6/D7/D9**. Nicht gleichzeitige Seitenabtastung berücksichtigen.
5. **F0–F4** nur nach Kontrolle und **nur RAM**; keine dauerhaften Motorparameter/kein F5. Bei unbekannten R/L/Flux nicht blind auf den Motor losfahren.
6. Android-STOP `E6` prüfen und bei Bedarf mehrfach senden. Sofortiges Abschalten *in der Firmware* muss separat geprüft werden.

## 3. Erstes E5 nur nach Aufhebung aller Sperren

1. Startstrom exakt **100 mA** (zulässiger Bereich für diesen Kandidaten 100–500 mA; Default 250 mA ist **nicht** der erste Test).
2. E5 einmalig betätigen; D6/D7/D9 und Bus-/Phasenstrom beobachten.
3. Auf **Rattern, Sprung in falsche Richtung, hohes Iq, Fehler 0x0C01 / 0x0C02, Observer-Verlust** sofort E6 und physische Abschaltung, falls E6 nicht greift.
4. **Kein automatisches Hochregeln**, keine Fahrt. Erst Log auswerten, dann über Wiederholung entscheiden.

## 4. Erwarteter Testbericht

- Modell/ESC-Hardware / 10S-Busspannung / Firmware-Build / Android-App-Version
- BLE pairing, MIC-Bad-Zähler, D0-D9, E0-Status, D9 faults
- Messkanal-Offsets PA3/PA4/PA5, Gate/BKIN-Selbsttest, Verhalten nach E6
- Auslösen des unabhängigen Firmware-Timeouts und Verhalten bei BLE-Abbruch
- Ergebnis: **PASS nur bei kontrolliertem Radlauf und nachweislich sicherem Stop; sonst FAIL und keine Fahrfreigabe.**
