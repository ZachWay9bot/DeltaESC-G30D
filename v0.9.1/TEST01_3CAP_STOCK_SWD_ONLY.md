# Hardwaretest 01 – G30D Gen1 3-Cap, SHFW installiert, SWD nur lesen

**Prüfaufbau (vom Besitzer bestätigt, 2026-10-10):** originaler 3-Cap-G30D STM32-Controller, vergossen, MT8006A, verstärkte MOSFETs/Kondensatoren, kein ADC-Mod, Stern/Dreieck laut Betreiber bei 60 V lauffähig. ST-Link und Oszilloskop vorhanden. **Kein strombegrenztes Labornetzteil.**

## Kein unnötiges Hardware-Raten

Es wird **kein weiteres Platinenfoto** verlangt und kein freier MCU-Testpunkt unter dem Verguss vorausgesetzt. **Aktuell ist SHFW installiert, NICHT originale DRV126. SHFW nicht für diese Messung ersetzen.** 60-V-Betrieb mit bisheriger Firmware liefert **keine** 60-V-Qualifikation für DeltaESC bzw. eine ungeprüfte ADC-Skalierung.

## Erster auszuführender Test

Nur an einem sicher fixierten, stillstehenden Controller mit installierter SHFW: über STM32CubeProgrammer **SWD HOTPLUG** und **-r32** GPIOA_CRH / GPIOA_IDR / GPIOA_ODR / GPIOB_CRL / GPIOB_CRH / GPIOC_CRH / GPIOC_IDR / TIM1_CCER / TIM1_BDTR auslesen. Für das vollständige Registerbild und optionale GPIOC_IDR-Tastenprobe siehe vorhandene Dateien `v0.9.1/G30D_STLink_ReadOnly_Preflight.ps1` und `v0.9.1/HARDWARE_PREFLIGHT.md`.

**Befehl in PowerShell:** `powershell -NoProfile -File .\G30D_STLink_ReadOnly_Preflight.ps1 -Phase Idle` (Skript herunterladen, ggf. Pfad zur STM32_Programmer_CLI.exe mit `-Cli` setzen). Für Tastenvergleich optional kurzer `-Phase Button`-Leseaufruf; bei verfehltem Moment ist ein konstanter Wert **kein Beweis** gegen PC14. Niemals lange für die Messung gedrückt halten.

**Interpretation:**
- PC14 Pin 3 (GPIOC_IDR Bit 14) und PA12 (GPIOA_IDR Bit 12): konkurrierende Power-Button-Hypothesen, mit Kurz-Tastentest eingrenzen.
- PA11 (GPIOA_ODR Bit 11) und GPIOA_CRH: Power-Hold-Softwarezustand, **kein direkter Spannungsnachweis**.
- PB1-Modus in GPIOB_CRL: BEMF-/Gate-Enable-Kontroverse; GPIO-Modus ist kein Leitungsplan.
- TIM1_CCER und TIM1_BDTR: aktuelle PWM-/Break-Register, nicht tatsächliche MOSFET-Gate-Polarität oder physikalisch qualifizierte Schutzfunktion.

**ST-Link-Probe verbindet ohne Reset/Halt laut ST UM2237 im HOTPLUG-Modus; Verbindung selbst bleibt ein möglicher Eingriff.** Bei Fehler, Bewegung, Reset oder unklarer Versorgung sofort stoppen; nicht mit Under Reset, RDP-Unlock, Flash-Write oder Erase „reparieren“.

## Oszilloskop bewusst zurückgestellt

MT8006A und MCU sind vergossen. Ohne eindeutig zugänglichen sicheren Niederspannungsbezug, geeignete Messisolation und strombegrenzten Aufbau **keine Gate-, Phasen- oder 60-V-Zwischenkreismessungen mit geerdetem Tastkopf**. Für diesen Test ist das Oszilloskop nicht erforderlich.

**Ergebnis:** Die TXT-Logs (Idle und optional Button) erfassen ausschließlich die SHFW-Laufzeitregister. Ein Vergleich gegen den bereits vorliegenden DRV126-Fulldump ist ein separater OFFLINE-Schritt; die GPIO-Modi dieser beiden Firmwarestände dürfen nicht verwechselt werden. Ein schwankender Button-Pegel kann die elektrische Route eingrenzen, aber nicht alle Schaltungsfunktionen beweisen. **Kein aktives PWM, kein Motor Detect und kein 60-V-Test.**
