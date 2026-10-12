# DeltaESC G30D v0.9.18 – ADC-Gültigkeitsnachweis im vorhandenen BLE-Motor-Bench

**SOURCE ONLY / NO FLASH / NO MOTOR ENABLE**  
Basierend auf v0.9.17 (EBiCS MOE-Latch), v0.9.14 VESC-Motor-ID und
DRV126-ADC-Rekonstruktion v0.9.8. HW: originaler vergossener G30D Gen1
STM32F103 3-cap; keine strombegrenzte 10S-Labornetzteilversorgung.

## Die tatsächliche Änderung

- Zwei neue **ausschließlich lesbare** Ninebot/NinebotCrypto-Escape-Register
  `0xFA` und `0xFB`, jeweils **18 Byte little-endian**.
- Anbindung an die **bereits vorhandenen echten** ADC-/Kalibrier-RAM-Daten:
  drei Rohwerte des Gate-OFF-Fixed-Scans, ADC1-Nulloffsets, ADC2-Offsets
  separat für CH4/CH5, Zeit seit letztem ADC, CCER/BDTR, VBUS-Rohwert,
  letzten/Peak-Current-Count und latched Faultcode.
- Snapshot-Read nutzt ein kurzes IRQ-gesperrtes Kopierfenster, damit
  zusammengehörende ADC-Zeilen/Offsets/Bdtr/Fehlerstatus nicht durch die
  ISR halb überschrieben werden. Interruptstatus wird vorher gelesen und
  danach **nur wenn vorher freigegeben** wieder freigegeben.
- Alle Stromwerte ausdrücklich ADC-Counts, **keine kalibrierten mA**.
  Das originale DRV126-Integer-Skalierungsmodell existiert bereits aus
  v0.9.8: `(delta * 51575) >> 10` mit korrekter Signierung, ist jedoch
  **nicht** die am individuell vergossenen ESC vermessene physikalische
  Stromverstärkung.
- ADC-Gültigkeit unterscheidet Rohwert 0..4095, Offset nahe 2048±100,
  aktuell gemessenen rohen Nullstrom <=24 Counts, Datenalter <=100 ms,
  passive Kanalsperre `CCER=0x1000` und die gesetzte MOE-Kill-Verriegelung.
- **Kein** Veto der v0.9.17-Sperre aufgehoben. E7 und normaler Drive
  bleiben compile-blocked; keine künstlichen Motorstromwerte, kein
  automatisches Qualifizieren der Hardware.
- `FW_BUILD=0x0918` verhindert das irrtümliche Freischalten durch den
  bisherigen Android-v0.3.6-Bench-Guard für `0x0914`.

## Neue read-only BLE-Register

Alle `uint16` sind little-endian; Formatversion ist 1. Die Anwendung
muss die reguläre NinebotCrypto-Verbindung für READ verwenden.

| Offset | FA (18 Byte) | FB (18 Byte) |
| --- | --- | --- |
| 0 | Versionsbyte = 1 | Versionsbyte = 1 |
| 1 | Flag-Byte | Flag-Byte |
| 2–3 | ADC1 fixed raw[0] | ADC2 CH4 Nulloffset |
| 4–5 | ADC1 fixed raw[1] | ADC2 CH5 Nulloffset |
| 6–7 | ADC1 fixed raw[2] | zuletzt gemeldeter Strom-Rohcount |
| 8–9 | ADC1 Nulloffset[0] | max. Strom-Rohcount |
| 10–11 | ADC1 Nulloffset[1] | TIM1 CCER |
| 12–13 | ADC1 Nulloffset[2] | TIM1 BDTR |
| 14–15 | ADC-Alter ms (Sättigung 65535) | ADC VBUS-Rohcount |
| 16–17 | Safety-Fault Code | größter Abstand raw/offset in ADC Counts |

Flagbitmaske (beide Seiten identisch):

- `0x01`: mindestens eine ADC-Probe vorhanden
- `0x02`: drei ADC1-Offets gültig und jeweils 1948–2148
- `0x04`: getrennte ADC2-CH4/CH5-Nulloffsets gültig und plausibel
- `0x08`: drei raw-ADC1 Werte im 12-Bit-Bereich
- `0x10`: ADC1-Daten <=100 ms alt
- `0x20`: motorfrei im **registerbezogenen** Sinn (`!g_power_armed` und CCER=0x1000)
- `0x40`: v0.9.17 terminaler MOE-Kill-Latch gesetzt
- `0x80`: obige ADC1+Fresh+CCER-Prüfungen UND Peak roh <=24 Counts

Wichtig: Flag `0x80` bestätigt **nur** geringen Ruhezustands-ADC-Count.
Es bestätigt weder eine Strombegrenzung in Ampere noch die reale
MT8006A-Gate-Abschaltung.

## Verifizierbarkeit

Workflow: [v0.9.18 in GitHub Actions](https://github.com/ZachWay9bot/DeltaESC-G30D/actions/workflows/v118-adc-proof-ble.yml).
Er baut die komplette Quellhistorie v0.9.14 -> v0.9.16 -> v0.9.17 ->
v0.9.18, führt alle bestehenden Tests erneut aus, verifiziert die neuen
18-Byte-Protokoll-Pages auf Overflow/Drift/Stale/Offsets, Cortex-M3-Link und
die Verweigerung von E7/ID-Bench-Motorleistung. Das Image wird als
**SOURCE-ONLY-NO-FLASH** Artefakt für binäre Prüfung bereitgestellt.

Die Android Motor-Bench-App wurde mit v0.9.18 **noch nicht aktualisiert**;
sie müsste FA/FB lesen und den neuen `0x0918` Build erkennen. Diese Version
ist daher zunächst eine fertig integrierte Firmware-Diagnoseschnittstelle,
keine bestätigte BLE-App-End-to-End-Messung.

## Nächster echter Fortschritt nach Software

Aktive Motor-R/L/Flux-Messung erfordert eine reproduzierbare physische
Gate-Abschaltung plus eine unabhängige strom-/energiebegrenzte Messumgebung.
Ein normaler 10S-Akku mit BMS begrenzt den möglichen Kurzschlussstrom nicht
schnell genug, um diese Testabsicherung zu ersetzen. Die vergossene
Originalhardware ist für interne MT8006A-HIN/LIN-Messungen unzugänglich;
die Freigabe darf nicht durch das Vortäuschen eines gemessenen Stromgain
erzwingbar sein.
