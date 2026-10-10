# DeltaESC G30D: VESC/EBiCS F103 Sensorless-Observer-Port (v0.9.6)

**Umfang:** gezielter Austausch des vorläufigen BEMF-Winkel-Observers durch einen F103-Festkomma-Port des VESC-Ortega-Flussobservers. **Kein neuer Gate-, ADC- oder FOC-Entwurf.** Die vorhandene G30D/DRV126-ADC-zu-TIM1-Kette und gemeinsame Abschaltfunktion bleiben erhalten.

## Quellen und Herkunft

- VESC `vedderb/bldc`, [`motor/foc_math.c`, `foc_observer_update()`](https://github.com/vedderb/bldc/blob/master/motor/foc_math.c): Flussbeobachter ẋ = v − R·i + (γ/2)(x − L·i)(λ² − ‖x−L·i‖²), mit negativ geklemmter Radiusabweichung in der ursprünglichen Ortega-Variante. Upstream GPL-3.0-or-later.
- EBiCS [`Sensorless_VESC/Src/FOC.c`, `observer_update()`](https://github.com/EBiCS/EBiCS_Firmware/blob/Sensorless_VESC/Src/FOC.c): bereits vorhandene STM32F103-Interpretation des VESC-Beobachters ohne FPU, Festkomma- und 64-Bit-Zwischenwerte. EBiCS ist hier **Portierungsreferenz**, nicht blind eingebundene Hardwareabstraktion.
- EBiCS [`EBiCS_motor_FOC`](https://github.com/EBiCS/EBiCS_motor_FOC): F103-Festkomma-FOC-Referenz. Der DeltaESC-PWM-/FOC-Treiber selbst ist unverändert.
- Ninebot DRV126 `esc126_fulldump.bin`: eigener STM32F103-G30D-Pin-/ADC-/TIM1-Backend aus dem bisherigen Projektstand, inklusive DRV126-CCER-Registerpfad `0x1555`.

## Implementierung

Die Dateien `src/vesc_ebics_flux.[ch]` sind eine neue Integer-Adaption der **Ortega-Gleichung**, keine unveränderte Kopie des VESC-F4-Firmware-Builds. Rechenvariablen sind nWb, mV, mA, uOhm und nH; Regelrate 4 kHz. Im Sinne des EBiCS-F103-Ansatzes werden im Regelpfad keine `float`-Berechnungen und keine 64-Bit-Division benutzt. Die dimensionslose Beobachterkorrektur wurde begrenzt. Der Magnetflusswinkel wird aus dem Residuum `(x - L*i)` gewonnen und **nicht** erneut um 90° verschoben wie beim alten BEMF-Winkel.

`src/sensorless_control.c` bindet diesen Observer per `VESC_EBICS_OBSERVER=1` in den bisherigen Alignment/Open-Loop/Handover/Closed-Loop-Regelkreis ein. `motor_pipeline.c`, `stock_tim1_gate.c`, `stock_current_frontend.c` und TIM1/ADC-Registerkonfiguration in `main.c` bleiben unverändert, bis auf die neue Versionskennung im Banner. Fehler/Gas weg/Bremse gehen weiterhin über den gleichen STOP/Gate-OFF-Pfad.

## Reproduzierbarer Build

```bash
bash tools/run_v096_tests.sh
```

Der Patch `v096_vesc_ebics_port.patch` wendet sich mit `git apply` auf den vollständigen v0.9.5-Quellbaum an. Darin ist das Testskript enthalten. Der Quellstand ist auch als vollständiges ZIP verfügbar.

**Bestätigte Software-Resultate:** Cortex-M3 vollständig gelinkt, Quell-BIN `19.204` Bytes; 19 Hosttest-Gruppen bestanden, darunter simulierte Sensorless-Übergabe und Fehlerabschaltung; separater idealisierter Winkeltest `599/599` innerhalb 20°, bei 80 rad/s und λ=0,015 Wb, nach Einschwingzeit. Unterliegende Messgrößen sind in diesen Simulationen idealisiert.

**Nicht bestätigt:** Laufzeitbudget auf echtem STM32F103, Einhaltung von ADC-Trigger-Zeitpunkten am G30D, Phasenstromskalierung und reale Motorparameter. **Der reale motoraktive Build bleibt nicht freigegeben.** Diese Begrenzung hat nichts mit der bereits bekannten Gate-Pinbelegung oder dem Sensorless-Algorithmus selbst zu tun. Installierte SHFW wird nicht verändert. Kein Radlauf und kein fahrfähiger Release nachgewiesen.

**Lizenz:** VESC und EBiCS sind GPL-basierte Quellen; die neue Adaptionsdatei steht unter `GPL-3.0-or-later`, einschließlich Urheberhinweisen und Quellverweisen. Die VESC-Komplettfirmware wird hier ausdrücklich nicht als drop-in für den STM32F103 ausgegeben.