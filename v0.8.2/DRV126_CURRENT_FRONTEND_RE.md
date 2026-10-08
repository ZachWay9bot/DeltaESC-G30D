# DRV126 current frontend reverse-engineering notes

Source: exact 128 KiB stock dump SHA-256
`9235b466a2f7449b8184560dbef9012d7c12b099e4076100a223ef68d204bb68`.

The dump itself is not committed.

## Relevant stock addresses

- `0x080058E0`: runtime ADC injected setup.
- `0x0800590E`: ADC1 injected channel 3, rank 1, sample-time code 2.
- `0x0800591A`: ADC2 injected channel 5, rank 1, sample-time code 2.
- `0x08005734`: two-sample phase-current reconstruction.
- `0x08005766` etc: multiplier `0xC977`, arithmetic shift 10.
- `0x08005510`: SVM sector sign tree; writes sector into current-sampling state.
- `0x08005B2C`: updates ADC1/ADC2 JSQR channel pair from the sector.
- `0x08005B70`: temporarily removes ADC1 injected trigger selection, updates JSQR pair, restores TIM1_CC4 selection.
- `0x08001424`: ADC JEOC handler; runs motor control and then updates the next sector's ADC pair.
- `0x0800598C`: startup offset calibration path using ADC1 injected CH3/CH4/CH5 (plus a fourth non-phase channel).

## Recovered channel selection

The one-rank injected JSQR values in stock are:
- CH3 = `0x18000`
- CH4 = `0x20000`
- CH5 = `0x28000`

The `0x08005B2C` sector table selects:
- sector 1/6: ADC1 CH4, ADC2 CH5,
- sector 2/3: ADC1 CH3, ADC2 CH5,
- sector 4/5: ADC1 CH3, ADC2 CH4.

The reconstruction at `0x08005734` is equivalent, before the common stock scale, to:

- 1/6: `Ia=dB+dC, Ib=-dB, Ic=-dC`
- 2/3: `Ia=-dA, Ib=dA+dC, Ic=-dC`
- 4/5: `Ia=-dA, Ib=-dB, Ic=dA+dB`

where `dA/dB/dC` are raw phase ADC values minus the corresponding zero-current offset.

## What this proves and what it does not

This strongly fixes the software topology and sector mapping. It does not by itself prove real-board current polarity, analog gain tolerance, absolute A/count calibration, gate timing, or that the custom PWM sample point produces an equivalent analog sample. Those remain hardware commissioning blockers.
