# Hardwarehistorie und FO91-Rueckflashregression — 21.10.2026

## Verbindlicher aktueller Auftrag

ST-Link ist an diesen Controllern physisch nicht erreichbar und steht als
Rueckweg nicht zur Verfuegung. Bis auf Weiteres wird per IAP geflasht.
Der erwartete Lieferzustand des kommenden Controllers ist DRV 1.8.7;
DRV126 ist die geplante Basis. Der Lieferzustand ist eine Erwartung,
kein bereits ausgelesener Wert. Der Weg 1.8.7 -> DRV126 ist damit nicht
automatisch fuer den noch nicht eingetroffenen Controller bestaetigt.

Motorentwicklung ist freigegeben. Vorrang hat der dauerhaft erreichbare
Rueckflashpfad. Keine neuen Bootloader-Fingerprint-, nominalen
Flashgroessen-, Versions-, Kalibrierungs- oder Motorfehler-Gates fuer IAP.

## Historische Original-Logs erneut gelesen

Die folgenden archivierten Resultate wurden am 21.10.2026 aus der privaten
Issue-Historie gelesen. Es wurden keine neuen Jobs gesendet. Die Links
erfordern Repositoryzugriff; hier stehen nur die fuer die Zuordnung
notwendigen Ergebnisse, keine vollstaendigen privaten Logdaten.

| Versuch | Originalbeleg | Tatsaechliches Ergebnis |
|---|---|---|
| FO75 -> FO76, 17.10.2026 23:23:30 UTC | [fo76-repo-flash](https://github.com/ZachWay9bot/xbot-remote-lab/issues/1#issuecomment-6243026704) | `ok=true`, `verified=true`, `XBOTFO76V00761`, 21.712 Bytes, 170 Bloecke |
| FO76-Motor/Wizard, 17.10.2026 23:24:26 UTC | [fo76-24v-wizard-once](https://github.com/ZachWay9bot/xbot-remote-lab/issues/1#issuecomment-6243035433) | `success=true`, Wizard `DONE/PLATEAU`, Takeover `SUCCESS`, 40.000/40.000 ACTIVE-Ticks, max. 76 Strom-Counts, ausgewaehlter Observer 0 Clamps |
| Weiterflash aus FO76 | [fo78-flash-go](https://github.com/ZachWay9bot/xbot-remote-lab/issues/1#issuecomment-6251415339) | FO76 -> FO78, `ok=true`, `verified=true`; der IAP-Weg aus FO76 funktionierte |
| FO80 -> FO91/0.9.1 | [ebics-r91-flash](https://github.com/ZachWay9bot/xbot-remote-lab/issues/1#issuecomment-6254317220) | `XBOTFO91V00911` installiert, `verified=true`, 44.160 Bytes |
| Rueckflash FO91 -> FO80 | [fo91-recovery-to-fo80](https://github.com/ZachWay9bot/xbot-remote-lab/issues/1#issuecomment-6255849837) | `ok=false`, `RuntimeError: IAP status 7` |
| FO91 Bootfenster-Versuch | [fo91-bootwindow-recover-fo80](https://github.com/ZachWay9bot/xbot-remote-lab/issues/1#issuecomment-6275973512) | Kein IAP_OK; Status 7 in 2.155 Antworten |

Damit sind fruehere echte Flash-, Motor- und Weiterflash-Erfolge belegt.
Die Formulierung "kein Hardwaretest" gilt ausschliesslich fuer den neuen
DeltaESC-v0.10.3-Kandidaten, nicht fuer die gesamte Entwicklungsgeschichte.
FO76 bezeichnet hier die XBOT-Firmwarelinie; nicht den separaten
DeltaESC-Branch v0.7.6.

## Rueckflashfehler in FO91/0.9.1

Der archivierte [FO91-Befund](https://github.com/ZachWay9bot/xbot-remote-lab/issues/1#issuecomment-6278154923)
dokumentiert den Fingerprint `0x5E597808` im damaligen Image und Status 7
bei CMD07. Die zugehoerige r9-Quellrekonstruktion bindet die IAP-Berechtigung
an `boot_crc == 0x5e597808u && flash_kib == 128`.
Das ist ein dokumentierter Software-Lockout im Rueckflashpfad. Fehlende
Motorfreigabe erklaert ihn nicht. Der Controller kommunizierte weiterhin.

Die exakte urspruengliche FO91-Buildquelle ist nicht erhalten. Die spaetere
r9-Rekonstruktion und ihre Patches sind deshalb kein bytegleicher
FO91-Source-Restore und keine bereits ausgefuehrte Reparatur des gesperrten
Controllers. Einzelne fruehere Aenderungen an weiteren Rettungswegen sind
mit dieser Belegauswahl noch nicht vollstaendig rekonstruiert.

## Zwei unterschiedliche FO76-Artefakte sauber auseinanderhalten

Die Versionskennung allein identifiziert den tatsaechlich getesteten Code
nicht eindeutig:

- Tatsaechlich geflasht laut Original-Log: 21.712 Bytes, SHA256
  `35ceae5c635c66f667380fc8dd0777463aed5dde98014328d123be5965812554`.
- Spaeter bytegleich aus dem vollstaendigen Original-Source-Paket
  reproduzierter BENCH-Stand laut
  [Quellpruefung](https://github.com/ZachWay9bot/xbot-remote-lab/blob/main/FO72_SOURCE_ROLLBACK_BASELINE.md):
  21.728 Bytes, SHA256
  `a8b7ca7d45fdfa32cbcbfdfbe98e2e655a266ecc57aea62f8149ecf3cbe74bcb`.
- Beide melden `XBOTFO76V00761`. Deshalb darf der Hardwareerfolg nicht
  allein aufgrund dieser Kennung auf die abweichende Source-Reproduktion
  oder auf DeltaESC v0.10.3 uebertragen werden.
- Das verfuegbare FO76-Source-ZIP hat SHA256
  `298a6624d729e7b3a71ba8787c3b9da05196251d74488584a22992886f18f97c`.
  Der konkrete 16-Byte-Unterschied zur damaligen Flashdatei ist noch zu
  vergleichen; daraus folgt keine neue Runtime-Sperre.

Die Messungen bleiben wertvolle echte Referenz. Ihre damaligen
Testmotor-/Benchbedingungen werden erhalten; daraus werden keine
ungeprueften Parameter fuer den kommenden G30-Motor abgeleitet.

## Konsequenz fuer die laufende DeltaESC-Arbeit

Die funktionierenden IAP-/UART-/Staging-Schnittstellen bleiben Referenz.
In v0.10.3 sind diese Quelldateien gegen v0.10.2 bytegleich geprueft.
Das ist kein Anlass, den historischen FO91-Lockout erneut als
"Kompatibilitaetspruefung" einzubauen. Die vorliegenden Original-Logs
bleiben Teil der Basis fuer weitere Regressionen. Die konkrete
Hardwarepruefung des neuen Controllers und des neuen Kandidaten steht
noch aus; die frueheren Erfolge werden dadurch nicht ungeschehen.
