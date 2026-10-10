# DeltaESC G30D v0.9.9: Automatischer Motor-Detect-Ablauf (Software)

**Scope ausschließlich Punkt 2:** Anregungs-/Messsequenz für R, L, Flux und atomare Übernahme der Resultate. Basis ist der eingefrorene v0.9.8 ADC-/Strompfad. TIM1/ADC/FOC/Gate-Treiber wurden dabei funktional nicht neu geschrieben.

## Wirklich implementiert

- `src/motor_id_auto.[ch]` implementiert einen Ereignisautomaten am ADC-/Regeltakt. Er sendet über den Hardwareadapter (`vector_mv`) angeforderte Spannungsvektoren und isoliert die Gates über `gate_off`.
- **R:** Ausgehend von kleinem Vektorsollwert wird die Anregung in Schritten erhöht, bis ein definiertes Stromfenster erreicht ist; nach Settling werden acht gekoppelte Spannungs-/Strompaare zur bestehenden `motor_id_accept`-Auswertung weitergereicht.
- **L:** Acht getrennte Spannungsimpulse, dazwischen physischer Gate-Off und bestätigter kleiner Ruhestrom. Der tatsächliche Impulsabstand wird zur `di/dt`-Berechnung verwendet.
- **Flux:** Rotierender angeforderter Feldvektor, danach Gate-Off und Erfassung der tatsächlichen BEMF- und Rotor-Drehzahlwerte. Die *angeforderte* Rotationsgeschwindigkeit wird **nicht** als gemessene Geschwindigkeit ausgegeben. Ohne echte BEMF-/Geschwindigkeitserfassung wird die Messung abgebrochen.
- ADC-Samples tragen `command_epoch`; alte Samples oder fehlende ADC-Gültigkeit sperren sofort. Bremse, fehlende Bedienfreigabe, Treiber-/ADC-Fehler, Überstrom-Probewerte und Regeltakt-Timeout führen über `gate_off` zu FAULT.
- Erst nach R, L und Flux, bei ausgeschalteten Gates und gestopptem Regler, werden Parameter in einer **RAM-Transaktion** gleichzeitig in `sensorless_control_t` und optional `motor_config_txn_t` übernommen. Ein `result_applied`-Callback kann BLE-Caches aktualisieren. Keine Flash-Schreibzugriffe.
- Die Motor-ID-Runtime ist in `src/main.c` initialisiert und unter `F7` (Parameter) und neu `F8` (Sequenzstatus) **lesbar**. Diese v0.9.9-Ausgabe ist bewusst nicht über BLE startbar.

## Tests

`bash tools/run_v099_tests.sh`: 23 Hosttestgruppen und vollständiger Cortex-M3-Link bestanden (inkl. bisherigen 22). Die neue Gruppe simuliert eine vollständige R-/L-/Flux-Anregungsfolge einschließlich der Gate-Off-Übergänge, ungültiger Messung, fehlender realer Rotor-Geschwindigkeit, Bremse, fehlender Hardwarefreigabe, Regelzeitlücke und transaktionaler Parameterübernahme. Source-only STM32F103-Image: siehe Testprotokoll, **nicht flashen**.

## Was NICHT als fertig gelten darf

Dieser Build besitzt bewusst **keinen freigegebenen ESC-Motor-Detect-Hardwareadapter**. Auf dem echten G30D fehlen weiterhin die verifizierten Hardwarefunktionen zur Befehlsausgabe und zur kalibrierten Rückführung von Phase-Spannung, Phase-Strom und insbesondere gemessener BEMF/elektrischer Drehzahl während der Flux-Phase. In `main.c` ist `vector_mv` daher `NULL`. Die vorherigen Freigabesperren `COMM_CURRENT_SCALE_HW_VALID=0` und `COMM_ADC_TIMING_HW_VALID=0` bleiben unverändert; aktive Builds schlagen wie zuvor beim Kompilieren fehl. **Kein realer automatischer Motor Detect, keine Motorfreigabe, keine Fahrfreigabe.**

Das ist keine erneute Pinout- oder Regelkernentwicklung, sondern eine abgeschlossene host-getestete Ablaufsteuerung mit einem noch nicht angeschlossenen physischen I/O-Vertrag. Eine reine Software-Simulation ersetzt keinen motoraktiven Motor Detect.

## Quellen

- Bestehender DeltaESC-v0.9.8-Regler, `src/motor_id.c` und `motor_config_txn.c`.
- VESC `vedderb/bldc`, `motor/mcpwm_foc.c` (`mcpwm_foc_measure_resistance`, `mcpwm_foc_measure_inductance`, `mcpwm_foc_measure_res_ind`), als Konzeptvorlage für R-Rampe und L-Spannungsimpulse; kein direkter STM32F4-Treiberimport.
- EBiCS-F103 für Rechen- und Peripheriekontext. Keine Aktualisierung von VESC-, EBiCS-, Gate- oder ADC-Algorithmen in diesem Schritt.