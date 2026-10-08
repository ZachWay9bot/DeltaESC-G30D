# DeltaESC G30D v0.8.2 — stock DRV126 current frontend model

**SOURCE-ONLY. GATES COMPILE-DISABLED. NOT A FLASH OR ROAD RELEASE.**

v0.8.2 continues from the CI-green v0.8.1 DashBLE commissioning guard and replaces a major software assumption with the current-sampling topology recovered from the user's exact stock DRV126 dump.

## Stock DRV126 evidence

The original DRV126 firmware does not run the FOC current loop from one ADC scanning CH3/CH4/CH5 sequentially. It does this instead:

- startup offset calibration: ADC1 injected sequence measures CH3, CH4 and CH5,
- runtime: ADC1 and ADC2 perform injected conversions simultaneously,
- one injected rank per ADC,
- the two selected phase channels change with the SVM sector,
- the missing third phase is reconstructed from Ia + Ib + Ic = 0.

Recovered runtime phase pairs:

| SVM sector | ADC1 | ADC2 |
|---|---|---|
| 1 / 6 | CH4 | CH5 |
| 2 / 3 | CH3 | CH5 |
| 4 / 5 | CH3 | CH4 |

The stock current conversion uses multiplier 0xC977 (51575) followed by arithmetic >>10. v0.8.2 records that exact integer transform without claiming its physical unit has been hardware-validated.

## v0.8.2 software delta

- Adds `stock_current_frontend.c/.h`.
- Adds the exact DRV126 sector sign tree.
- Adds exact one-rank JSQR channel encoding used by the stock firmware.
- Adds sector-dependent ADC-pair selection.
- Adds phase-current reconstruction in raw ADC-delta space.
- Adds host truth-table/regression tests for all six sectors.
- Build identity becomes `0x0802`.
- D9 byte 11 bit7 reports that the stock current *software model* is compiled in.
- The v0.8.1 commissioning blocker `COMM_CURRENT_SCALE_HW_VALID` remains **0**.

## Safety boundary

This revision does **not** yet switch the live STM32 ADC ISR to the dual-ADC topology. That is deliberately separated from the recovered model so the model can be reviewed/tested independently first.

The next step is a v0.8.3 source-only acquisition layer that mirrors the stock ADC1/ADC2 injected-simultaneous setup and the post-control sector/channel handoff. No motor permission is granted by v0.8.2.
