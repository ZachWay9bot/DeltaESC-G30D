# DeltaESC G30D v0.9.11: Motor Detect Punkt 2.2, reale ADC-Rueckfuehrung (softwareseitig)

Basis: v0.9.10, separater Patch. Scope **ausschliesslich** das Ermitteln und Uebergeben kohaerenter Motor-ID-Messwerte. Keine neuen Power-Freigaben, keine neue BLE-Startaktion, kein BEMF/Flux-Anschluss.

## Verbunden

- `src/motor_id_adc_feedback.[ch]`: Hardware-synchronisierter Messvertrag. Die drei geschriebenen TIM1-CCR-Werte werden zunaechst nur in `queued` eingetragen. Erst das **physische TIM1-UP-Interrupt-Ereignis** veroeffentlicht sie als `active` mitsamt `command_epoch` und DWT-Zeitbezug. Ein altes UIF wird nach den CCR-Writes geloescht, damit eine vor dem Schreibvorgang stattgefundene Uebernahme nicht als neue ausgegeben wird. Nach Gate-Off wird das Snapshot sofort ungueltig.
- `src/main.c`: TIM1_UP IRQ25 ergaenzt; ADC1_2 IRQ18 nimmt echte ADC1/2-JDR1-Rohwerte, JEOC-Flags, JSQR, aktuelle Sektorwahl, getrennte ADC2-Kanaloffsets, DWT-Zeitstempel und real erfasste ADC-Busspannung auf. Unpassende oder veraltete Paare werden verworfen.
- `applied_voltage_mv` ist **nicht** die angeforderte Spannung, sondern die d-Achsen-Projektion des aus der *aktuell wirksamen* SVPWM-Duty und der tatsaechlich ADC-gelesenen Zwischenkreisspannung **rekonstruierten** Spannungsvektors. Dies ist keine separate Direktmessung der Phasenspannung am Motorstecker; reale FET-Spannungsabfaelle/Deadtime bleiben unkompensiert.
- `phase_current_ma` kommt von physisch abgetasteten beiden Phasenshunt-ADCs, DRV126-Sektorreonstruktion, separatem ADC2 CH4/CH5-Offset, Clarke plus Projektion auf den angelegten elektrischen Winkel und vorhandener **nominaler** DRV126-Stromskalierung. Begrenzung aller drei rekonstruierter Stromphasen vor Projektion, damit Quadraturstrom nicht verborgen bleibt.
- Gültige Paare gehen mit `command_epoch`, DWT-us-Timestamp und echter Busspannungsrechnung direkt in `motor_id_auto_tick()`; Bremse, Fault und STOP bleiben Fail-Closed. In Gate-Off-Rest-/Coast-Abschnitten kann der vorhandene echte dreikanalige Diagnose-Scan einen gemessenen Nullspannungs-Datensatz bereitstellen. Es werden ausdrücklich **keine** BEMF- oder Geschwindigkeitswerte erfunden.

## Harte Grenzen

- `COMM_CURRENT_SCALE_HW_VALID=0`, `COMM_ADC_TIMING_HW_VALID=0`, `POWER_STAGE_ARM_ALLOWED=0`: **Motor-Firmware nicht freigegeben**. Die analoge Skalierung und der TIM1-CC4-Abtastzeitpunkt muessen am echten G30D qualifiziert werden, bevor echte Messwerte im aktiven Leistungsbetrieb nutzbar sind. Die Compile-Freigabesperre bleibt bestehen. Der Hosttest nutzt definierte synthetische Registerwerte; er ist **kein** Motorversuch.
- Die End-to-End-Erfassung wurde auf einem Host-Simulator und als Cortex-M3-Code nachgewiesen; sie konnte mangels Hardwarefreigabe **nicht** auf einem real energisierten G30D verifiziert werden.
- Punkt 2.3 (echte BEMF und elektrische Drehzahl im Flux-Coast) und Punkt 2.4 (Motor-Detect-Startfreigabe/Hardwareversuch) bleiben getrennt offen.

## Reproduzierbarkeit

`patch -p1 < v0.9.11/v101_motor_id_adc_feedback.patch` auf dem v0.9.10-Quellstand; `bash tools/run_v101_tests.sh` im Projektroot.

25 Hosttestgruppen bestanden, ARM Cortex-M3-Link erfolgreich, safe BIN 23.892 Byte. Die Strom-/ADC-Timing-Gatechecks verweigern weiterhin aktivierbare Firmware. Readme und Testlog dokumentieren ausschließlich Softwareverifikation.
