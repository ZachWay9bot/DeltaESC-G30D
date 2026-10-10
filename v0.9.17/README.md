# DeltaESC G30D v0.9.17 – EBiCS-inspirierter verriegelter TIM1-MOE-Stopp

**11.10.2026 · SOURCE ONLY · NO FLASH · KEIN MOTOR-RELEASE**

Dieser Branch ergänzt den von der originalen DRV126- und der EBiCS-Referenz abgeleiteten TIM1-Abschaltpfad. Der eingefrorene BLE/NinebotCrypto-Transport, die VESC/EBiCS-Observer-Algorithmen und der v0.9.14-Motor-ID-Quellcode bleiben unangetastet. Der originale G30D Gen1 ESC ist vergossen, MT8006A HIN/LIN können nicht zugänglich gemessen werden.

## Umsetzung

- `v0.9.16` hält die **passive ADC-Diagnose** offen: TIM1 CH4 ist aktiv, sechs CH1/2/3- und N-Kanal-Freigaben sind AUS, AOE/BKE/BKP/LOCK=0. Wegen TIM1/ADC-Trigger kann MOE hier 1 bleiben. Das ist **nicht** gleichbedeutend mit einer elektrischen MOSFET-Abschaltbestätigung.
- `v0.9.17` schaltet **beim ersten Aufruf des gemeinsamen `power_stage_force_disarm()`** terminal ab. Das umfasst E6, Power-off, Motor-Pipeline STOP/Brake, Faults und Watchdogs, soweit sie den vorhandenen gemeinsamen Stopp-Pfad verwenden. 
- Der Pfad setzt zuerst `g_gate_kill_latched=1`, trennt die sechs TIM1-Leistungskanäle via `CCER`, löscht anschließend `BDTR.MOE`, setzt alle sechs GPIO-Ausgänge über ODR=0 und Eingangs-Pull-down auf passiv und kontrolliert **Register- und GPIO-Rücklesewerte**.
- Bei Rücklesefehler latched Fault `0xA02C`; zusätzliche Laufzeitprüfung im `gate_safety_check()` hat Fault `0xA02D` und wiederholt die Abschaltung. Kein automatisches MOE-Wiederanlaufen.
- Ein solcher STOP ist **terminal bis zum MCU-Reset**. Passive CH4-ADC-Diagnose kann dabei ausfallen. Keine BLE-Funktion löscht diesen Hardware-Kill-Latch.
- Das erneute Initialisieren des TIM1 nach dem Stop wird abgewehrt, ebenso ein `motor_hw_arm()` oder eine nachfolgende CCR-Torque-Schreibanforderung.
- Source-only erzwungen: `POWER_STAGE_ARM_ALLOWED=1`, `SENSORLESS_RUN_ALLOWED=1` und `MOTOR_ID_BENCH_ALLOWED=1` erzeugen Compilerfehler. Ein v0.9.17-Gate-Motor-BENCH-Image ist **nicht** erzeugbar.

## Build und Tests

CI: [GitHub Actions v0.9.17, erfolgreich](https://github.com/ZachWay9bot/DeltaESC-G30D/actions/runs/38090533059).

1. Rekonstruktion des vollständigen v0.9.14 Motor-ID-Codes aus geprüfter v0.9.0-Archivbasis samt aller Patch-Stufen.
2. v0.9.14: Cortex-M3 + 26 Hosttestgruppen, beide vorhandene Profile vor der v0.9.17-Überlagerung.
3. v0.9.16: TIM1-BDTR/CCER/GPIO-Pull-down-Vektortests und source-only ARM-Build.
4. v0.9.17: 131072 Registervarianten, mehrfacher Kill, acht abstrakte Stop-Gründe, stock `BDTR=0xCD40` wird nicht als sicherer Kill akzeptiert.
5. v0.9.17: STM32F103 ARM-Build, < 53248 Byte aktives App-Limit, prüfbare Quellankerpunkte. Negativer Buildtest verbietet aktivierbares `id-bench`.

Der Host-Test simuliert Registerzustände, nicht die reale MT8006A-Gate-Abschaltung. **CI-GRÜN IST KEINE FREIGABE DER MOTORLEISTUNG.**

## Physisch noch offen (bei vergossenem ESC)

Der tatsächliche Gate-Off-Pegel und das analoge AMP_LIM/PB12-Verhalten sind weder aus TIM1-SWD-Idle-Werten noch aus Firmwarecode elektrisch nachweisbar. Ein indirekter Phasenausgangstest wäre keine vollständige Prüfung des internen schnellen Überstromschutzes. Für einen tatsächlichen 10S-Motor-Ersttest wäre ein energielimitierter Prüfstand mit unabhängiger Strombegrenzung und abgesicherter Messung erforderlich. Kein Flash oder Motorstart aus diesem Branch.

Referenzen:
- [EBiCS-Submodul von SmartESC v3, Commit e5648e4](https://github.com/EBiCS/EBiCS_motor_FOC/blob/e5648e4b8e5736f32cc713b8b8e22e2826953229/motor.c), `motor_disable_pwm()`: löscht TIM1 BDTR.MOE.
- [SmartESC v3](https://github.com/Koxx3/SmartESC_STM32_v3), `Core/Src/main.c`: benutzt die EBiCS-Abschaltung im Fehlerfall.
- [EBiCS/SmartESC genauer Gate-Audit in v0.9.16](../v0.9.16/EBICS_SMARTESC_V3_GATE_AUDIT.md).
