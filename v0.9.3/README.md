# DeltaESC G30D v0.9.3 – Motor Detect R/L/Flux (Quellstand, Gate-OFF)

**ENTWICKLUNGSSTAND / NICHT FLASHEN / KEIN AKTIVER MOTORLAUF**

Basis: eingefrorene v0.9.0 + separate v0.9.1-GPIO-Korrektur + v0.9.2-ADC/FOC-Stromkontrakt. Die v0.9.3-Änderungen sind auf diese Basis anwendbar, ohne eingefrorene Versionen zu bearbeiten. **SHFW bleibt auf dem realen G30D installiert.**

## Neu implementiert

- `src/motor_id.[ch]`: hardwareunabhängiger, dreistufiger Motorparameter-Ermittler aus **kalibrierten** Messwerten (R in µΩ, L in nH, Fluss in µWb). Die Stufen heißen RESISTANCE, INDUCTANCE, FLUX, READY und FAULT.
- Der R-Pfad fordert stationären Rotor, signierten positiven Messstrom und eine gültige Spannung-/Strompaarung. Der L-Pfad schätzt `L = (V-RI)*dt/di`. Der Flux-Pfad nutzt `lambda = E/omega_e` und setzt eine valide elektrische Drehzahlinformation voraus. **Das ist keine Aussage, dass der Rotor-/Geschwindigkeits-Beobachter bereits physisch qualifiziert ist.**
- Je Stufe mindestens acht konsistente Werte, maximale Abtast-/Zeitgrenzen, monotoner Eventzähler inklusive Überlauf, Plausibilitätsfenster und sofortiges FAULT bei Bremse, STOP, unvollständigen Daten oder schlechter Messqualität.
- Die Ergebnisse werden nur bei vollständig bestätigter Hardwarequalifikation, konsistenten Messreihen und stillstehendem Regler zur Sensorless-Parameterkonfiguration zugelassen. Ein Stop oder Fault sperrt den Commit.
- `0xF7` als **neue rein lesende** 16-Byte-BLE-Seite: State/Fault/Valid/Qualification (=0), danach R/L/Flux als LE32. Die bestehende App muss für die Interpretation dieser Seite später erweitert werden.
- Für den STM32F103 ohne libgcc wurde die 64/32-Bit-Division explizit als gebundene Integerarithmetik umgesetzt. Der **Motor-ID-Code wird beim Cortex-M3-Build erzwungen gelinkt**, sodass fehlende `__aeabi_*`-Funktionen den Build tatsächlich scheitern ließen.
- Die Aktivierung der Leistungsausgänge bleibt **hart zur Compilezeit gesperrt**. `0xF7` startet keine Messung und erregt keine Motorphase.

## Getestet

1. Sauberer Cortex-M3-ELF/Syncsafe-BIN-Link mit erzwungener Einbindung von `motor_id_accept` und `motor_id_commit` ohne unaufgelöste Runtime-Symbole.
2. **16 Hosttest-Gruppen** durchgelaufen: bisherige 14 Gruppen plus Motor-ID-Prüfungen und eine geschlossene PMSM-Plant-Simulation mit synthetischen Messwerten und negativen Sicherheitsfällen.
3. Die PMSM-Simulation prüft Alignment, Open-Loop, Observer-Handover und Closed-Loop sowie Gas-weg und gleichzeitige Abschaltung beim Observer-Verlust. Sie nimmt eine idealisierte Rotorfolge an und beweist keinen realen Start.
4. Tests überprüfen die Drei-Stufen-Verkettung, Einheiten, Grenzwerte, fehlende Hardwarefreigabe, STOP/Bremse, Sequenzfehler, Wertestreuung, Parametrierung nur im STOP-Zustand und sperrende Fehlerverriegelung.
5. Beide konfigurierbaren Motor-Aktivierungen werden beim Kompilieren weiterhin abgewiesen. Flash-Image unter der 52-KiB-Partition.
6. Reproduzierbarer Patchtest gegenüber dem unveränderten vollständigen v0.9.2-Quellstand.

## Nicht behauptet, nicht implementiert

- **Kein automatisches Erregen/Ansteuerungssequenz** für R-/L-/Fluss-Messungen auf der physischen MT8006A-Leistungsstufe; bisher nur Berechnung und Freigabeprotokoll.
- Keine echte PWM-synchrone 1-Rank-Sektor-ADC-Umschaltung; der v0.9.2-Kontrakt ist noch nicht physikalisch angebunden.
- Keine bestätigte Gate-Polung, Hardware-Break/OC-Protektion, ADC-Shuntgain oder gemessene reale Motorskalierung. 60V erfolgreicher SHFW-Betrieb beweist diese Eigenschaften für DeltaESC **nicht**.
- **Kein freigegebener 10S-Motorstart, kein fahrfähiger RC.** Ein Original-ST-Link-Rückweg ersetzt keine Leistungsstufensicherung.

## Weiterarbeit

Nächster Codepunkt: softwareseitig vollständigen zeitlichen TIM1-Update/ADC-Trigger/Auswertungspfad und unabhängigen Gate-Break-Abschaltpfad verbinden. Vor dem aktiven 10S-Bench bleiben die elektrische Qualifikation und die Begrenzung der Testenergie unumgänglich. Stock-/SHFW/OTA werden nicht verändert.

Lokale Prüfung:

```bash
ARM_CC=clang ARM_OBJCOPY=llvm-objcopy ARM_OBJDUMP=llvm-objdump bash tools/run_v093_tests.sh
```