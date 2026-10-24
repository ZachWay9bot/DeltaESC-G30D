# DeltaESC G30 Gen1: ein gemessenes Profil, ein Fahr-Build

Das bekannte Ziel ist der **G30D Gen1, 15 Polpaare**. Die Verbindung zum neuen Controller ist real bestätigt: COM9, Stock DRV187. Die Quelle liegt unter `g30/`; der bisherige DeltaESC DRIVE-Quellstand ist softwaregebaut, aber das Default-G30-Profil enthält **R/L/Flux/Umfang = 0** und kann deshalb keinen Motorlauf auslösen. Die alten FO7x-Tests werden nicht neu durchgeführt.

## Einmalig benötigte physische G30-Werte

Die gültigen Messungen dieses konkreten G30 Gen1 in eine JSON-Datei eintragen:

```json
{
  "motor": "G30 Gen1",
  "measurement_evidence": "Datum, Messung oder Logdatei des echten Gen1",
  "r_uohm": null,
  "l_nh": null,
  "flux_uwb": null,
  "phase_offset_u16": null,
  "pole_pairs": 15,
  "wheel_circumference_mm": null,
  "test_current_ma": 500
}
```

`null` ist bewusst NICHT buildfähig, solange keine echte Messung existiert. FO70/FO76-Werte gehörten zum anderen Prüf-Motor und dürfen nicht als G30-Werte ausgegeben werden. Phasenfolge und ADC-Stromskala zusätzlich am physisch richtigen G30-Controller zuordnen; **ein Firmwareprofil ersetzt keinen Nachweis, dass FETs und Gates elektrisch richtig arbeiten**.

## Unveränderte Firmware einmal mit Profil bauen

Auf einem Buildhost mit `make`, `clang-18`, `llvm-objcopy-18`, `llvm-objdump-18`:

```sh
python3 bench/G30_Gen1_Measured_Build.py --profile g30_gen1_measured.json --validate-only
python3 bench/G30_Gen1_Measured_Build.py --profile g30_gen1_measured.json
```

Der Builder legt in einem temporären Quellverzeichnis den **originalen** `make drive-runtime`-Build mit `PROFILE_CFLAGS=-DG30_PROFILE_...` an. Die Firmwarequelle, `g30/src/iap_update.c`, Linker und bestehende IAP-Logik werden nicht verändert. Kein SHU, kein direkter Serial-Port, kein Flash, kein Bootloader-Update und keine neue Flash-Sperre.

Ausgabe: `out/g30_gen1_measured/DeltaESC_G30D_v0_10_3_Gen1_measured.bin`, ELF und `manifest.json` mit tatsächlich benutzten Motorkonstanten, SHA256, Größe, Stack- und Startadresse. Ein erneuter Profil-Build ist nur bei **geänderten Messwerten** erforderlich, nicht für jeden Test.

**Grenze:** Solange R/L/Flux/Phasenbezug und realer analoger Strompfad nicht vorliegen, wäre ein „fahrbereiter BIN“ mit geratenen Parametern unzuverlässig und könnte die Leistungsstufe schädigen. Auch der unveränderte FO7x-IAP-Parser im eigenen Image ist kein Beleg, dass DRV187 bereits dieses Image bootet. Der erste konkrete Firmware-/Motorlauf kann nur am angeschlossenen Controller unter lokaler Aufsicht erfolgen.

## Aktueller Fortschritt
- Real: UART / 115200 / COM9 und DRV187 (0x0187) bestätigt.
- Software: Gas, Brake=Coast, Wiederanlauf und Sensorless-FOC vorbereitet; 15 pp als bestätigte G30-Konstante.
- Neu: Reproduzierbarer *einmaliger* Profil-Fahr-Build ohne erneute Source-/IAP-Änderungen.
- Offen: reale G30-Messwerte im JSON, Einbau in den echten Build und ein kontrollierter Rad-frei-Motorlauf. Nicht erneut die 1636 alten FO7x-Tests durchführen.
