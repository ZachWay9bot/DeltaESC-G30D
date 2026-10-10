# DeltaESC G30D v0.9.8 — DRV126-abgeglichener ADC-/Strompfad

**Scope ausschliesslich Punkt 1: ADC-Trigger / Stromrekonstruktion / Kanaloffsets / Skalierung / Vorzeichen.**
Freeze v0.9.6 und der bestehende v0.9.7 Entwicklungsstand bleiben unveraendert. Keine
Aenderung des VESC-Observers, Motor Detect, Leistungstreibers oder Ninebot-BLE-Transports.

## Direkter Original-Firmware-Nachweis (esc126_fulldump.bin)

Die DRV126-Referenz stammt aus der frueher im Projekt bereitgestellten Datei mit
131072 Bytes und SHA256 `9235b466a2f7449b8184560dbef9012d7c12b099e4076100a223ef68d204bb68`.
Der Originaldump wird bewusst **nicht mitgeliefert oder in GitHub gespeichert**.
`tools/audit_drv126_adc_stock.py` validiert offline neun konkrete Thumb-Fingerprints,
Registerwerte und Tabellen in genau dieser Datei.

**Belegte Maschinenprogramm-Anker:**

| Adresse | Original-Befund | v0.9.8-Umsetzung |
|---|---|---|
| `0x08005734–0x080057B8` | ADC1 JDR1 + ADC2 JDR1, sektorbezogene Vorzeichen | Identische 3 Sektorpaare; gesonderter exakter Golden-Test |
| `0x08005766`, `0x08005786`, `0x080057A4` | Multiplikation `0xC977=51575`, danach signed `ASRS #10` | `stock_current_drv126_scaled_delta` mit korrekter negativer Abrundung |
| `0x08005B2C–0x08005B60` | Sektor-Umschaltung beider JSQR-Einzelranks | Sektor 1/6: ADC1 CH4 + ADC2 CH5; 2/3: CH3 + CH5; 4/5: CH3 + CH4 |
| `0x080048B8–0x080048CE` | TIM1 PSC=0, ARR=4000, CMS=1, RCR=1 | Gleiches Timer-Grundprofil |
| `0x080048D8` | CH4-Modus PWM1, initial CCR4=3800 | CH4 PWM1 statt bisherigem PWM2 |
| `0x08005712–0x08005724` | Laufzeit-CCR4=3996, CCER=0x1555 | CH4-Trigger `CCR4=3996`; TIM1-Registervertrag prueft die gesamte Konfiguration |
| `0x08005AC4–0x08005B18` | Nullpunkt-Pruefung jedes ADC-Kanals nahe `2048±100` | Gleiche Offset-Plausibilitaetsgrenze |

Die nachgebildete **Stock-interne Stromskala** `delta * 51575 >> 10` ist nun
vorzeichen- und rundungsgetreu. Eine Umbenennung dieser Integerwerte in **physikalische mA**
setzt aber den Shunt und den Analogeingangsverstaerker voraus; dieses elektrische
Verstaerkungsverhaeltnis ist fuer den individuell umgebauten ESC durch das reine
Maschinenprogramm nicht nachgewiesen. Die nominale mA-Konversion wird deshalb
nur mit dieser Einschränkung angeboten.

## Reparaturen am bisherigen ADC-Pfad

1. **Beide ADCs besitzen eigene Nulloffsets:** ADC2 CH4 und CH5 werden mit je 256
   Gate-OFF-Samples separat kalibriert, jeweils gegen 2048±100 validiert und in
   die ADC1-Bezugsebene ueberfuehrt. Fehlender oder ueberlaufender Offset sperrt die Probe.
2. **Originales Timerprofil statt Naeherung:** PWM ARR4000, ADC CH4 PWM1 und CCR4=3996,
   PSC0, Center-Aligned1, RCR1; 8-kHz-Timertraeger bei 64 MHz, FOC weiter bei 4 kHz
   (Divider 2). ADC-Fenster-Pruefung kennt die anschliessende Down-Count-Phase.
3. **Korrekte Stromsignale:** Sektor-Gruppen und JSQR wurden direkt im Stock-Dump
   identifiziert, mit 348486 synthetischen Goldvektoren inkl. negativer Werte bestaetigt.
   Summierte Phasenstromwerte duerfen bei >4095 Counts nicht auf 0 fallen.
4. **Validitaetskette:** gleiche injizierte ADC1/ADC2-Rank-Ereignisse, JEOC, JSQR,
   Triggerkonfiguration, Samplezeiten, Duty-Fenster und Offset-Flags muessen passen,
   ehe die Messung an FOC weitergereicht wird. Fehler fuehren zu STOP.

## Verifizierung

```
# Ohne Originaldump: alle Host- und ARM-Build-Tests
bash tools/run_v098_tests.sh

# Zusaetzlich die Original-Thumb-Bytes gegen den Nutzer-Dump pruefen
DRV126_STOCK_BIN=/pfad/esc126_fulldump.bin bash tools/run_v098_tests.sh
```

- **22 Hosttest-Gruppen bestanden**, darunter die neue Stock-Referenz mit **348486**
  Datensaetzen. `gcc -fsanitize=undefined` fuer Host.
- Vollstaendiger STM32F103 Cortex-M3-Link, **20096 Bytes GATE-OFF-BIN** unter 52 KiB.
- 9 Thumb-/Tabellenanker gegen den echten Originaldump per SHA + Bytes bestaetigt.
- Motor-Enable Builds mit `POWER_STAGE_ARM_ALLOWED=1` oder `SENSORLESS_RUN_ALLOWED=1`
  schlagen weiterhin absichtlich fehl.

## Praezise Abschlussgrenze

**Softwareseitiger DRV126-Abgleich fuer ADC-Kanalpaare, Vorzeichen, Integer-Faktor,
TIM1-Abtastprofil und getrennte Nulloffsets ist umgesetzt und offline getestet.**

Dies ist **KEIN elektrischer Nachweis**, dass die physische Analogverstaerkung des
umbebauten ESC genau zur Stock-mA-Skala passt oder die PWM-synchrone ADC-Phase
unter realer Motorlast stabil ist. Der konservative Zeitfenster-Grenzwert von
252 TIM1-Ticks ist ein DeltaESC-Schutzvertrag, keine aus DRV126 belegte Naturkonstante.
`COMM_CURRENT_SCALE_HW_VALID=0` und `COMM_ADC_TIMING_HW_VALID=0` bleiben deshalb
auf 0; es wird kein unqualifizierter Motor-Build freigegeben.

**Keine weiteren allgemeinen Pinout-Runden; keine Aenderungen am Roller, SHFW bleibt.**