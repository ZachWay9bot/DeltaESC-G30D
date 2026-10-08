# DeltaESC G30D v0.7.2 — BLE Motor Probe (SOURCE ONLY)

## User-selected access contract

**ST-Link = recovery only. Configuration and diagnostics = Android DashBLE → the original Ninebot Dashboard BLE (NinebotCrypto handshake) → PA2 USART2 half-duplex → ESC.** Do not require SWD readout, serial debug, or a second radio during normal testing.

v0.7.2 is a conservative additive protocol extension to CI-green v0.7.1. The legacy D0–D9, E0–E6, F0–F5 and NinebotCrypto / dashboard handling are not modified. The 256-sample ADC statistics already collected in firmware are now available by six **read-only** Ninebot ESC 0x20 registers (command **0x01 READ**, 0x04 READ_ACK), all 16 bytes, little-endian:

| Register | Bytes 0–3 | 4–7 | 8–11 | 12–15 |
|---|---|---|---|---|
| DA | ASCII MTR0 | sequence (u32) | completed windows (u32) | last window timestamp ms (u32) |
| DB | mean ADC0–1 (u16×2) | mean ADC2–3 (u16×2) | min ADC0–1 (u16×2) | min ADC2–3 (u16×2) |
| DC | max ADC0–1 (u16×2) | max ADC2–3 (u16×2) | last ADC0–1 (u16×2) | last ADC2–3 (u16×2) |
| DD | offsets ADC0–1 (u16×2) | offset ADC2 / window size | gate-armed flag (u32) | total ADC samples (u32) |
| DE | sample min cycles (u32) | sample max cycles (u32) | max FOC control cycles (u32) | TIM1_CCER (u32) |
| DF | ADC1_JSQR (u32) | ADC1_CR2 (u32) | TIM1_BDTR (u32) | protocol version<<16 \| window-valid bit |

A bounded three-attempt sequence-consistent snapshot is used to avoid tearing between ADC interrupts and BLE reads. Poll **only on the motor-diagnostic screen**, ideally ~1 Hz; preserve the app v0.1.0/v0.1.1 login and existing D0–D9 polling. Until DashBLE's UI is extended, these pages can be read through its raw Ninebot register feature.

The v0.7.1 FOC PI correction and passive ADC statistics are retained. **Gates are compile-disabled.** No E5 arm/drive, no phase PWM, no automated active R/L or Flux measurement and **no physical motor start**. R/L/Flux fields in F0–F2 are still configuration records, not validated model calibration and are not yet wired into the observer.

**Important:** This is built and exercised in a synthetic ARM UART/ADC environment, not on the physical user's ESC. Do not infer that the source-only package is a flash/recovery-approved firmware. The original DRV126 readout-protection/mass-erase and stock-IAP failure hazard remain. ST-Link is reserved for recovery and must not be used for routine motor parameter/debug reads.

Next work: DashBLE UI page showing DA–DF in usable units, preserving verified NinebotCrypto v0.1.x transport. Then real ADC characterization from the phone before hardware gated PWM is reconsidered.
