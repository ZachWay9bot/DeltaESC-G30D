# FO69 / FO70 -> DeltaESC port map

| XBOT evidence / function | DeltaESC v0.10.0 destination | Status |
|---|---|---|
| FO70 PA6/PA7/PB1 passive BEMF ranks | `g30_rolling_bemf` + target ADC callbacks | ported |
| FO70 raw forward sector step = 5 | `g30_rolling_bemf_config.forward_raw_step` | ported, configurable |
| FO70 3 km/h kick gate | `min_speed_kmh10=30` | ported, hardware calibration pending |
| FO70 4 coherent edges / 70 ms freshness | rolling BEMF defaults | ported |
| FO70 3x channel averaging | `g30_passive_bemf_adc` | ported |
| FO69 negative electrical forward direction | rolling BEMF step + start policy | ported |
| FO69 phase-step guard | `ROLL_STEP_*` / rolling policy | ported and 4 kHz-scaled |
| FO69 flux magnitude 0.5..1.5 lambda | `rolling_flux_instant_ok()` | ported |
| FO69 current phase <45°, avg <30° | `SENSORLESS_ROLL_VERIFY` | ported |
| FO69 quiet clamp delta = 0 | `vesc_flux.clamp_events` + ROLL_VERIFY | ported |
| FO69 modest Iq takeover | zero-Iq verify + 250 ms ramp, 12-count cap | ported |
| FO69 active coast-gap compensation | not needed for passive pre-arm FO70 path | intentionally not copied |
| FO69/XBOT current calibration/trips | G30 current qualification | NOT copied |
| FO69/XBOT deadtime/polarity | existing stock-G30 TIM1 audit | NOT copied |
| XBOT UART/IAP | DeltaESC Ninebot/IAP | already present; not duplicated |
