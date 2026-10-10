# DeltaESC G30D Gate-Ansteuerung: EBiCS + SmartESC v3 als Referenzen

Status: 2026-10-11. **Quellenabgleich, kein Motor-Release, kein Hardwaretest**.
Basis: v0.9.16 SOURCE-ONLY. Originaler G30D Gen1 ESC ist vergossen:
kein direkter Zugang zu MT8006A HIN/LIN.

## Genaue Quellen und Versionsbindung

1. [SmartESC STM32 v3](https://github.com/Koxx3/SmartESC_STM32_v3), 
   `Core/Src/main.c`, `Core/Src/stm32f1xx_hal_msp.c`,
   `Core/Inc/config.h`. SmartESC v3 hat `Lib/EBiCS_motor_FOC`
   als Git-Submodule, **SHA `e5648e4b8e5736f32cc713b8b8e22e2826953229`**.
2. [Genau dieses EBiCS-Submodul](https://github.com/EBiCS/EBiCS_motor_FOC/blob/e5648e4b8e5736f32cc713b8b8e22e2826953229/motor.c).
   Für aktuelle EBiCS-Codeentwicklung zusätzlich [main/motor.c](https://github.com/EBiCS/EBiCS_motor_FOC/blob/main/motor.c).
3. [ST RM0008](https://www.st.com/resource/en/reference_manual/rm0008-stm32f101xx-stm32f102xx-stm32f103xx-stm32f105xx-and-stm32f107xx-advanced-armbased-32bit-mcus-stmicroelectronics.pdf)
   und [ST-Diskussion zum TIM1_CH4-ADC-Trigger (F103, Juni 2026)](https://community.st.com/t5/stm32-mcus-embedded-software/how-to-trigger-adc-with-timer-cc-event-using-stm32f103/td-p/823856).
4. v0.9.15 DRV126-Eigenmessung: `TIM1_BDTR=0xCD40`,
   `TIM1_CCER=0x1000`, in stock idle.

## Nachvollzogene Codepunkte

| Funktion | EBiCS (auch SmartESC-v3 Submodul) | Konsequenz für DeltaESC |
| --- | --- | --- |
| Physischer PWM-Stop | `motor_disable_pwm()` löscht `TIM1->BDTR.MOE` | Zusätzlich zu CCER/GPIO eine separate Notabschaltung vorsehen |
| PWM-Start | `enable_pwm()` setzt `MOE` | Nur nach komplett neuem Guard/State-Machine-Arm, niemals nach einem latched Fault |
| Automatisches Wiederfreigeben | `TIM_AUTOMATICOUTPUT_DISABLE` | `AOE=0` in DeltaESC beibehalten |
| Timer LOCK | `TIM_LOCKLEVEL_OFF` | `LOCK=0` in DeltaESC beibehalten, vor Schreiben geprüft |
| Break | `TIM_BREAK_DISABLE` | **Kein** Nachweis, dass PB12/AMP_LIM auf dem G30 so verwendet werden darf |
| PWM-Kanäle | TIM1 CH1/2/3 + CH1N/2N/3N, auf PA8/9/10 und PB13/14/15 | Bestätigt Zuordnung, die im G30D durch DRV126-SWD bereits beobachtet wurde |
| ADC-Timing | TIM1 CH4, `TIM_TRGO_OC4REF` im EBiCS TIM1-Init | ADC-Triggerpfad ist **nicht automatisch gleich** mit DeltaESC TIM1_CH4/JEXTSEL |
| Totzeit | EBiCS `DeadTime=32` | **Nicht kopieren.** DRV126 `DTG=0x40` (64); Timer-Takt + Leistungsstufe maßgebend |
| Fehlerhandling | SmartESC `Error_Handler` ruft `motor_disable_pwm()` in Endlosschleife | Latched Fault ohne automatisches Rearm ist die richtige Semantik |
| Board-Pins | SmartESC M365 konfiguriert PC14 PWR_BTN und PC15 TPS_ENA | **Nicht** auf G30D übernehmen: PC14/PA12 hatten in G30 SWD Button-Tests keinen Pegelwechsel |

## Wichtiger Unterschied MOE und ADC

Auf STM32F103 wird der ADC-injected-Trigger über `TIM1_CH4` anders
verschaltet als EBiCS `TIM_TRGO_OC4REF`. Laut ST-Community kann bei
TIM1_CH4-Triggerung `CC4E` und `BDTR.MOE=1` für den ADC erforderlich
sein. **Daher nicht EBiCS `MOE=0` als permanenten Diagnosezustand kopieren.**

Vorschlag: zwei getrennte Zustände im künftigen Hardware-Build:

- `PASSIVE_ADC`: `CCER=0x1000`, `MOE=1`, `AOE=0`,
  alle sechs PWM-Pins als Eingang/Pull-down; nur Diagnostik, nicht
  als nachgewiesenes physisches Treiber-Off bewerten.
- `FAULT_LATCHED`: `CCER` CH1..3/CHxN abgeschaltet, sechs
  Pins Pull-down, zusätzlich `MOE=0` wie EBiCS. ADC-CH4-Trigger
  darf dabei ausfallen. Fehlerquittierung darf kein automatisches
  Wiederanlaufen erzeugen. Falls eine Wiederaufnahme von Diagnostik
  nötig ist, muss sie ausschließlich durch einen eigens geprüften
  Rückweg in PASSIVE_ADC geschehen.

**Noch nicht eingebaut:** `MOE=0`-Notabschaltung in allen echten
Firmware-Fault-Zweigen. Dieser Bericht belegt die Upstream-Methode,
ersetzt aber keine geänderte Firmware oder ein Testprotokoll.

## Was nicht übernommen wird

- `BreakState=DISABLE` aus EBiCS nicht als Sicherheitsnachweis ausgeben.
- `BreakPolarity=HIGH` ohne Messung von G30D PB12/AMP_LIM nicht übernehmen.
- EBiCS `OCIdleState=SET` nicht ungeprüft an aktive HIGH
  MT8006A A-Version HIN/LIN-Ausgänge übertragen.
- Keine G30D-Funktionsgarantie aus SmartESC v3 ableiten: ursprünglicher
  M365-Port, trotz gleicher STM32F103-Timerperipherie.

## Nächste Implementierung

`FAULT_LATCHED` in einen eigenständigen C-Helper zur atomaren
Softwareabschaltung integrieren. Bei Hardware-/Softwarefault zuerst
alle sechs CCER-Kanäle ausschalten, Pins herunterziehen, MOE
löschen, Registerreadback kontrollieren und Wiedereinschalten
verriegeln. Den normalen ADC-Diagnosepfad getrennt halten.
Nur Source-/Host-Tests; ohne physische Treibermessung kein 10S-Motor-Release.
