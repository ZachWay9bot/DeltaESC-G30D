# DeltaESC G30D v0.9.14 – VESC-Messablauf auf STM32F103

**Neu implementiert, nicht bloß ein Versionswechsel.** Basiert auf v0.9.13, vorhandener DRV126-Gate/ADC-/PWM-Anbindung und eingefrorenem NinebotCrypto-Transport.

## VESC-Vorbild
- vedderb/bldc `motor/mcpwm_foc.c` (SHA `aeff0ed34c8d87c8fac36a4ec49a82f8873e94fe`): Widerstands-Stromrampe, HFI-SIX_VECTOR-Induktanzmessung, Stromabhängigkeit, Ergebnisse erst nach vollständiger Messung übernehmen.
- vedderb/bldc `conf_general.c` (SHA `9e55aa470ba5971d38aad50f55e20a483a5611fa`): Ausrichten, gesteuerte Open-Loop-Beschleunigung und Flux-Messung aus tatsächlich gemessenem Strom/Spannung/elektrischer Geschwindigkeit.
- Diese Dateien stehen unter GPL-3.0-or-later. Die Algorithmen und das Ablaufkonzept wurden für den F103 portiert, *nicht* der STM32F4-HFI-/ChibiOS-Treiber 1:1 eingefügt.

## Firmware-Ablauf im E7-Motor-Bench-Modus
1. **R:** Bei 50 mV beginnen, in 25-mV-Schritten bis zum tatsächlich gemessenen Zielfenster 600–1200 mA steigern. 16 synchronisierte Spannungs-/Strompaare mitteln. Hartes Maximum 750 mV.
2. **L:** Aus gemessenem R die erste Impulsspannung für ca. 500 mA bestimmen. Sechs elektrische Orientierungen, vier Umläufe = 24 Stromanstiegsimpulse mit jeweils 4 ms Gate-Off-Ruhezeit und 1 ms Anregung. Strom im Anschluss nachregeln, nicht starr 1 V einspeisen. Ruheprüfung nutzt ALLE drei Shuntphasen. Für L zählt die tatsächliche TIM1-CCR-Übernahmezeit, nicht die Callback-Zeit.
3. **Flux:** Ausrichten 150 ms, elektrische Open-Loop-Geschwindigkeit linear von 1,5 auf 15 rad/s in 950 ms steigern, 220 ms setzen lassen, dann 12 gültige Messfenster zu je 20 ms. Spannung beginnt bei 75 mV und passt sich mit begrenzten 25-mV-Schritten alle 4 ms an den tatsächlich gemessenen Strom an. Flux wird aus modellierter BEMF aus V(alpha/beta), I(alpha/beta), R und L und der beobachteten elektrischen Drehzahl geschätzt, NICHT aus Sollgeschwindigkeit. Ungültiger Stillstand/Fehlmessung führt zu Abbruch.
4. **Übernahme:** R/L/Flux gemeinsam in RAM anwenden; keine Flash-Parameter-Schreibaktion.

## G30-spezifische Einschränkung gegenüber VESC
Der G30 mit 4-kHz-ADC und zwei Shunts bietet **nicht** das VESC-HFI-Six-Vector-FFT-Verfahren mit dessen PWM-/ADC-Timing. Die sechs langsamen Winkelimpulse identifizieren eine mittlere Induktivität; sie ersetzen KEINE echte Ld/Lq-FFT. Die rückgeführte BEMF ist aus PWM/ADC und R/L rekonstruiert, nicht direkt als freie Phasenspannung gemessen. Die verwendete nominale DRV126-Stromskalierung und der reale Motorlauf sind nicht am Fahrzeug vermessen.

## Bench-Hardwarepfad
- `MOTOR_ID_BENCH_ALLOWED=1` bleibt ein separater Build mit tatsächlichem TIM1-Output bei authentifiziertem E7.
- `E6`: Stop. Bremse, ungültiger ADC, Rohstrom > 24 Counts, Dash-Stale, IRQ/ISR-Watchdog: physisches Gate-Off.
- Maximal 2500 ms zusammenhängende Leistung, maximal 3600 ms pro E7-Durchlauf. Kein regulärer E5-Fahrbetrieb.
- `FW_BUILD 0x0914`, Android v0.3.6; alte Android v0.3.5 mit exaktem 0x0913-Guard kann diesen Build nicht starten.

## Build und Prüfbarkeit
- `bash tools/run_v114_tests.sh`: bestehende 26 Software-Testgruppen, ARM Cortex-M3 Build beider Varianten.
- `make safe id-bench CC=clang OBJCOPY=llvm-objcopy OBJDUMP=llvm-objdump`
- Quellpatch auf unveränderte v0.9.13-Kopie angewandt, alle Änderungen byteidentisch und beide ARM-Images bitidentisch nachgebaut.
- **Kein realer Hardwaretest wurde hier durchgeführt.** Die Software-Rekonstruktion belegt nicht physikalische Motor-/Schaltsicherheit. Das aktive Image ist ein experimentelles Bench-Image, keine fahrbereite Release-Firmware.
