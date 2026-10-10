# DeltaESC G30D v0.9.10: Motor-Detect-Punkt 2.1, `vector_mv()` → SVPWM → TIM1

**Scope:** ausschliesslich den bisher fehlenden Motor-Detect-Spannungsvektor-Ausgabepfad anschliessen. Basis ist der unveraenderte Freeze v0.9.9. Keine geaenderten R/L/Flux-Algorithmen, keine neue Motor-Detect-Startaktion und keine veraenderten BLE-Befehle.

## Neu verbunden

- `main.c`: `motor_id_auto_io_t.vector_mv` ist nun `motor_id_hw_vector_mv` statt `NULL`; `motor_id_vector_io_t` nutzt **existierende** `motor_hw_select_adc`, `motor_hw_pwm`, `motor_hw_arm` und die gemeinsame Hardware-Gate-Abschaltung. Keine erfundene PB1-Gate-Freigabe.
- `sensorless_id_voltage_to_pwm`: Festkomma-Frontend fuer den **bestehenden** `vector_to_pwm`-SVPWM; Alpha/Beta-Transformation aus elektrischer Winkelvorgabe und Spannungsamplitude, gemessene VBUS-Spannung. Unzulaessige Amplituden bzw. Busspannungen werden *abgelehnt*, nicht verdeckt gekappt. Kein FPU-Code.
- `motor_id_vector_output_request`: prueft Qualifikationen, waehlt den DRV126-ADC-Sektor, programmiert den zugehoerigen Dual-ADC-JSQR-Paarvertrag **vor** der Gate-Freigabe, faehrt zuerst neutralen TIM1-Vektor, setzt danach die drei CCR-Preloads. Rueckfall/Fehler immer ueber Gate-Off, nicht per 50%-Duty. Achtet auf vorhandene ADC-Shunt-Abtastfenstergrenzen.
- Getrennte Motor-ID-PWM-Besitzkennung. Regulärer Motor-FOC und Motor Detect koennen TIM1 nicht gleichzeitig belegen. Das ADC-IRQ erkennt Motor-Detect-Besitz und prueft dabei JEOC, Kanalwahl und rohe Ueberstromwerte, ohne den normalen Drive-FOC aufzurufen oder fiktive Motor-ID-Messwerte zu erzeugen. Hardware-Vector-Callback arbeitet atomar gegen IRQ, beruecksichtigt die alte PRIMASK-Lage. Watchdog trennt die Leistungsausgaenge nach >12 ms ohne neuen Detect-Vektorbefehl. Jeder asynchrone Gate-Off loescht auch den gespeicherten Besitz-/Armed-Zustand.

## Reproduzierbare Resultate

- `bash tools/run_v100_tests.sh`: **24 Hosttestgruppen bestanden** (23 vorherige + neuer Ausgabetest mit 256 Winkeln, 0/90/180/270-Grad-Vorzeichen, Sektor-PWM/ADC-Plan, Re-Arm, Fehlerkaskade). GCC UBSan.
- STM32F103 Cortex-M3 Build **22.192 Bytes**, inkl. Motor-Detect-Vektor-Adapter, unter der 52-KiB-Zielregion.
- Zusätzlicher *nur im Temporaerkopie-Verzeichnis* ausgefuehrter ARM-Compile-Test bestaetigt den syntaktischen Codepfad mit `POWER_STAGE_ARM_ALLOWED=1` und simuliertem Setzen der beiden HW-Qualifikationsflags. Diese modifizierte Kopie ist **nicht** im Paket enthalten und ist **kein** fahrbarer oder flashbarer Build.
- Gegen v0.9.9 reproduzierbarer Git-Patch; Test, dass alle gepatchten Dateien bitgenau dem neuen Quellstand entsprechen.

## Sauber abgegrenzt: Was dieser Punkt NICHT liefert

Dies ist der **softwareseitig voll angeschlossene Spannungsvektor-Ausgabepfad**. Der benutzbare reale Puls am Scooter wurde **nicht** nachgewiesen. Das unveraenderte reguläre Build hat `POWER_STAGE_ARM_ALLOWED=0`, `COMM_CURRENT_SCALE_HW_VALID=0` und `COMM_ADC_TIMING_HW_VALID=0`. **Nicht flashen oder als motoraktiven Release bezeichnen.** Die echte Motor-Detect-Messdatenrueckfuehrung (`motor_id_auto_tick` mit kalibrierten V/I/BEMF-Daten) bleibt eine separate Aufgabe; sie wird hier gerade NICHT vorgespielt. SHFW auf dem Roller bleibt unangetastet. Beim G30D unter 10S/60V keinesfalls eine Sicherheitsfreigabe aus Simulationstests ableiten.

**Lizenz:** GPL-3.0-or-later entsprechend dem bisherigen DeltaESC/VESC/EBiCS-Projektstand.