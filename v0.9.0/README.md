# DeltaESC G30D v0.9.0 — erster Motorlauf: Stop- und Fault-Logik

**SOURCE ONLY · GATES AUS · NICHT FLASHEN · MOTOR NOCH NICHT FREIGEGEBEN**

Baseline: eingefrorene v0.8.12 (Build 0x080C). Entwicklung ohne Änderung dieses Freeze-Branches. Build-Kennung der neuen Software: **0x0900**.

## Konkreter Fehler im bisherigen Motor-Regelkern

In v0.8.12 konnte die Sensorless-Zustandsmaschine während der OPEN_LOOP-, HANDOVER- oder CLOSED_LOOP-Verarbeitung einen Fehler auslösen und dennoch **im selben Aufruf weiter in die PI-/PWM-Berechnung laufen**. Erst beim folgenden Aufruf würde der FAULT-Fall die Sollwerte neutralisieren. Bei 4 kHz Regelrate entspricht das einem weiteren möglicherweise bestromten Regelzyklus.

Auch `sensorless_control_stop()` setzte zwar den Zustand auf STOP, neutralisierte aber die drei gespeicherten CCR-PWM-Sollwerte **nicht unmittelbar**. Beim echten Motor wäre ein zusätzlich ausstehendes PWM-Update inakzeptabel.

## Umsetzung v0.9.0

- Nach jeder Zustandsautomaten-Transition wird **vor** der PWM-Berechnung geprüft, ob FAULT gesetzt wurde. Falls ja: Sollwerte noch im selben Aufruf neutralisieren, Integratoren zurücksetzen, Antriebswunsch löschen.
- `sensorless_control_stop()` setzt die CCR1/CCR2/CCR3-Sollwerte unmittelbar auf eine gemeinsame 50-%-Referenz und nutzt dafür den zuletzt verwendeten Timer-ARR-Wert. **Wichtig: 50 % ist nur ein rechnerischer Nullspannungsvektor, kein stromloses Ausrollen!** Physisches Motor-OFF erfordert gesperrte Gate-Treiber, unabhängig von diesen Sollwerten.
- Ein latched Observer-Fault kann nicht durch einen erneuten simplen Drive-Request umgangen werden. Gezielte Freigabe-/Resetlogik muss separat entworfen und hardwaregeprüft werden.
- Unbewaffnet/stoppend wird der Back-EMF-Observer nicht weiter mit unbrauchbaren ADC-Werten gespeist.
- Die tatsächliche STM32-Firmware hält weiterhin alle sechs TIM1-Leistungsausgänge und Motor-Arm-Builds compile-seitig deaktiviert. BLE und die vorhandenen read-only Diagnose-ABIs bleiben unverändert.

## Nachweis statt bloß neuer Versionsnummer

Das neue `first_spin_safety_host_test.c` scheitert reproduzierbar mit dem **ursprünglichen v0.8.12-Quellcode** (beim initialen Neutralwert), **besteht jedoch mit dem Fix**. Die Hostsimulation prüft Stop während OPEN_LOOP, unbewaffneten Observer, wechselnden Timer-ARR, Timeout bei nicht qualifiziertem Observer, sofortigen PWM-Neutralwert **noch im auslösenden Fehlertakt**, sowie latched-Fault-Rearm-Sperre. Die vorhandenen Testvarianten voltage/foc/sensorless bleiben aktiv.

GitHub Actions rekonstruiert den vollständigen v0.8.12-Quellcode über die unveränderten SHA-geprüften Overlays, setzt exakt den neuen Patch auf, kompiliert für **STM32F103 Cortex-M3**, verifiziert den absichtlich fehlschlagenden Gate-Enable-Build und führt sämtliche bisherigen ADC-, BLE-NinebotCrypto-, IAP- und Host-Regressionstests erneut aus. Export nur des Quellcodes, kein Flash-Image.

## Was uns für den ERSTEN MOTORLAUF tatsächlich blockiert

1. **Reale Recovery**: ST-Link SWD/Option-Bytes, passendes originales DRV126-Image und tatsächliches Stock-Rollback auf einem Test-ESC nachweisen. RDP-Entsperrung kann vollständiges Löschen bedeuten.
2. **Pinbelegung und Gate-Polarität**: PA8/9/10, PB13/14/15 und mutmaßliches PB1-Treiber-Enable sind derzeit Software-Annahmen. **Vor dem Bestromen jede Zuordnung mit Schaltplan und Oszilloskop bestätigen.**
3. **Hardware-Überstromabschaltung**: In v0.8.12 wird TIM1_BDTR **ohne BKE** konfiguriert. Ein echter unabhängiger Hardware-Break-Pfad zum Abschalten der MOSFETs ist noch **nicht nachgewiesen**. Für späteren aktiven Betrieb zunächst tatsächliche Schutzschaltung, BKIN/BKP-Polarität, BIF-Verhalten und `AOE=0` messen. Die Hardware-Break-Funktion kann den MOE-Ausgang unabhängig vom CPU-Code abschalten; sie darf nicht ohne gesicherte Verdrahtung blind aktiviert werden.
4. **Strommessung**: Die feste ADC-Zweirang-Erfassung aus v0.8.12 ist für Diagnose geeignet, aber nicht als synchrones FOC-PWM-Shuntmessfenster qualifiziert; ADC-Gain und Vorzeichen unbekannt. Die R/L/Flux-Parameter des tatsächlichen Motors müssen gemessen werden.
5. **Zuerst stromloser Prüfstand**: GATE-Enable elektrisch getrennt; Gate-Steuersignale und Totzeit messen; anschließend separat abgesicherte kleine Spannungs-/Stromversorgung, freies Rad, Not-Aus; später erst normaler 10S-Akku. Kein Fahrversuch während der Qualifikation.
6. **Gas weg / Bremse**: Die hier reparierte Software-Nullvektorlogik ist kein physisches Coast. Sobald PWM aktiviert wird, muss ein **separater hardwaregetesteter Gate-Disable-Pfad** Gas-weg und Bremse zuverlässig in Motor OFF umsetzen.

### Ohne Eingriff in deine Stock-Firmware: erste Hardwaremessung

Mit bereits angeschlossenem ST-Link lassen sich an einem **stehenden und sicher entlasteten Controller** die STM32-Timerregister beobachten: TIM1_CCMR1 (0x40012C18), TIM1_CCMR2 (0x40012C1C), TIM1_CCER (0x40012C20), TIM1_BDTR (0x40012C44), TIM1_CR2 (0x40012C04), GPIOA_CRH (0x40010804), GPIOB_CRH (0x40010C04). Das sind **nur Lesepunkte**, keine Flasherbefehle. Die Controller-Version und wirkliche Signalverdrahtung können daraus nicht vollständig abgeleitet werden; dafür sind Messungen am ESC notwendig.

**Diese v0.9.0 ist noch keine fahrfähige Release. Ein grüner Build ist keine elektrische Prüfung.**