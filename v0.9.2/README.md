# DeltaESC G30D v0.9.2 — sektorsynchroner ADC-/FOC-Motorpfad

**SOURCE ONLY · GATE-AUS · NICHT FLASHEN · KEINE FAHRFREIGABE**

Basis: v0.9.1 GPIO-Korrektur aus dem separaten Entwicklungszweig. Stock/SHFW bleiben auf dem realen Controller unverändert. FW-Buildwort für diesen Quellstand `0x0902`.

## Konkrete Umsetzung

- `foc_adc_contract.[ch]` verlangt **genau zwei gleichzeitige injizierte ADC1/ADC2 Rank-1-Messungen** im zugehörigen SVM/PWM-Sektor. Volle JSQR-Signaturen, beide JEOC-Bits, Rohniveau (0–4095), Triggersequenz und Sample-Zeitfenster werden geprüft.
- Nur wenn die Motorhardware zuvor **separat qualifiziert** ist, akzeptiert das Modell zugehörige Offsetwerte und rekonstruiert Phasenströme mit der bereits aus DRV126 modellierten Sektorlogik. Falsche, doppelte, fehlende oder veraltete Samples verriegeln die Software. Ein Reset ist erst bei ausgeschalteten Brückenkanälen erlaubt.
- `foc_adc_motor_handoff.[ch]` verbindet ein **gültiges** Phasenstrom-Tupel mit dem vorhandenen Sensorless-FOC-Regler und verlangt einen Gate-Kill-Callback. Bei ungültigem Sample, fehlendem Callback, inaktiver Hardwarefreigabe oder während des Regelaufrufs erkanntem Fault wird noch im selben Aufruf ein Hardware-Kill angefordert und der Regler gestoppt.
- **WICHTIG:** Das ist ein testbarer Hardware-API-Vertrag, **nicht** der Nachweis, dass der Kill auf dem MT8006A physisch funktioniert. Das bisherige **zwei-Rank-Diagnose-ADC-Schema** wird nicht als gleichzeitig nutzbare FOC-Strommessung ausgegeben.
- Staged `0x0902` im `main.c` und `Makefile`; aktivierbare Motor-Builds bleiben zur Compilezeit gesperrt. Weder SHU-/BLE-Update noch 60-V-Motorbetrieb werden hier verändert.

## Verifikation

- STM32F103 Cortex-M3 mit clang, ARM-Linker und ELF erzeugbar.
- Alle **14 Hosttest-Gruppen** bestehen (12 aus v0.9.1 + zwei neue).
- Neue Tests: 6 ADC-Sektorpaare, Sequenzüberlauf, 14 negative Fälle plus doppelte/übersprungene ADC-Events, Hardware-Kill-Callback-Veto und synchroner softwareseitiger Motorstopp.
- Beide Motor-Enable-Konfigurationen werden absichtlich bei der Kompilierung abgewiesen.
- Quelle wurde reproduzierbar aus dem v0.9.0-Actions-Archiv mit v0.9.1-Patch plus v0.9.2-Patch neu aufgebaut und getestet.

## Offene Schritte für einen echten Motorversuch

1. MT8006A-Gate-Shutdown und Break/Fault-Weg elektrisch beweisen (ohne zu raten, welcher GPIO "ENABLE" wäre).
2. ADC1/ADC2 injizierte *gleichzeitige* Ein-Rank-Erfassung im realen PWM-Sektor konfigurieren, gültiges Shunt-Messfenster für Sektor/Modulation definieren.
3. Stromoffset, Gain, Polarität, R/L/Flux sowie Motorpolpaare auf realer Hardware kalibrieren.
4. Niedrige, strombegrenzte Prüfstandsenergie, freies Rad, manuelle Abschaltung; erst danach 10S-Test. Der funktionierende 60-V-SHFW-Status ist KEINE Validierung für DeltaESC.
5. SHFW bleibt bis dahin installiert. Keine weiteren allgemeinen GPIO-Tests nötig.

Ausführen: `ARM_CC=clang ARM_OBJCOPY=llvm-objcopy ARM_OBJDUMP=llvm-objdump bash tools/run_v092_tests.sh` im entpackten Quellverzeichnis.

**Die Abnahme betrifft nur Software-Kontrakte und Hosttests. Kein echter Motorstart hat stattgefunden.**