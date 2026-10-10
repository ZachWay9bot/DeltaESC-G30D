# DeltaESC G30D v0.9.4 — durchgehender Motorpfad, gemeinsame Abschaltung

**Umfang ausschließlich:** ADC1/ADC2 → sektorabhängige Stromrekonstruktion → Sensorless-FOC → SVPWM → TIM1 CCR-Preloads → sechs TIM1-Gate-Ausgänge; eine gemeinsame Abschaltfunktion für Gas weg, Bremse, Watchdog, ADC- und FOC-Fehler.

## Konkret verbunden

- `src/motor_pipeline.[ch]`: exklusiver Ablaufbesitzer. Jedes TIM1-CC4-ADC-Ereignis wird mit vorher eingestellter Sektor-ADC-Paarung korreliert, beide JEOC-Zustände und den ADC-Rank geprüft, Sektorströme rekonstruiert und alle vier Ereignisse der Sensorless-Regelung zugeführt.
- `src/main.c`: Im **separaten Motorzweig** des ADC1_2-Interrupts werden die realen JDR1/JSQR/JEOC-Register an die Pipeline übergeben. Die nächste Sektor-Paarung wird erst nach dem abgeschlossenen IRQ-Messpaket programmiert. Der bestehende zweirangige, passive ADC-Diagnosemodus bleibt erhalten und kann nicht als phasensynchrone Strommessung verwendet werden.
- Sensorless-Regler führt Clarke/Park, PI, Observer und SVPWM aus; nach einem erfolgreichen Regeltakt werden die drei CCR-PWM-Werte an TIM1 geschrieben (Preload-Update durch TIM1). Die Hardware-Routinen schalten die sechs TIM1-Kanäle **nur hinter mehrfachen Compile- und Hardware-Sperren** in AF-Modus.
- `power_stage_force_disarm()`: zuerst `TIM1_CCER` Leistungskanal-Freigaben löschen und alle sechs Pins auf definierte passive Eingangskonfiguration zurückschalten. Dies ist der gemeinsame physische Abschalt-Pfad, nicht ein 50%-Nullvektor. Alle Pipeline-Fehler nutzen ihn.
- Direkte Abschaltung auch beim Decodieren von Bremse/Throttle, bei 250-ms-Dashboard-Stale via SysTick, bei ADC-/Observer-Fehler, rohwertiger Überstrom-Veto **vor dem nächsten CCR-Update**, ungültigen ADC-Kanälen und PWM-Schreibfehlern. Fehler werden für Re-Arm verriegelt.

## Testbeleg

- 17 Hosttest-Gruppen, inklusive des neuen **end-to-end Mock-Hardware-Tests mit zehn Stop-/Fault-Varianten**.
- Cortex-M3 / STM32F103 vollständiger Link inklusive `main.c`, `motor_pipeline.c` und FOC; Release-Image passt in 52 KiB.
- Motor-Enable-Builds bleiben **absichtlich compile-time verboten**. Das lokale BIN ist ein GATE-OFF-SYNC_SAFE-Image, keine fahrbare Firmware und **nicht freigegeben zum Flashen**.

## Noch keine physische Freigabe

Die neue End-to-End-Software ist integriert und auf simulierten Daten getestet. Das beweist **nicht**, dass die ADC-Abtastlage, Shunt-Vorzeichen und Strom-/Spannungsskalierung sowie die MT8006A-Gate-Polaritäten korrekt sind, oder dass physisch ein Motor abgeschaltet wird. Eine reine STM32-TIM1-CCER-Abschaltung ist **kein qualifizierter unabhängiger Hardware-Überstromschutz**. `COMM_CURRENT_SCALE_HW_VALID`, `COMM_GATE_HW_VALID`, `COMM_ADC_TIMING_HW_VALID` sind alle Null. SHFW bleibt auf dem Scooter, kein Flash, kein Motorversuch.

## Reproduzieren

```bash
ARM_CC=clang ARM_OBJCOPY=llvm-objcopy ARM_OBJDUMP=llvm-objdump bash tools/run_v094_tests.sh
```