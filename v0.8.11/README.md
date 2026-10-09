# DeltaESC G30D v0.8.11: Fixed two-rank current sensing diagnosis

**SOURCE ONLY. GATES OFF. NOT FLASH-READY OR MOTOR-READY.**

This candidate fixes the remaining dynamic-ADC-channel problem in v0.8.10.
The original ADC1/ADC2 ISR continuously rotated their injected JSQR
channels through sectors 1, 2 and 4. The new source programs the injected
sequence **once at boot** and NEVER rewrites ADC_JSQR/ADC2_JSQR in the
current IRQ. This removes an unverified STM32F103 dual-ADC synchronization
hazard without assuming this is a ready-to-run FOC frontend.

**STM32 RM0008 rank arrangement:** JL=01 selects injected JSQ3 then JSQ4.

| Rank | ADC1 master | ADC2 slave |
|---|---|---|
| 1 | CH3 / provisional A | CH5 / provisional C |
| 2 | CH4 / provisional B | CH5 second reading |

The two converters sample simultaneously **within each rank**, not
across both ranks. Therefore this is an exclusively **passive Gate-OFF
diagnostic acquisition**. The older sector-based reconstruction is not
used in the live ADC interrupt, and the motor control kernel can execute
its computational load but receives forced `power_armed=0`.

**D0 build: `0x080B`. New read-only EF 16B:**
0..5 CH3,CH4,CH5 raw u16; 6..11 three offsets u16; 12..13
second CH5 u16; byte 14 flags (bad mode, bad JSQR, invalid raw,
duplicate CH5 discrepancy, missing JEOC); byte15 offset calibration
running/valid flags. E0 collects 512 *valid* complete ADC1/ADC2
two-rank sample cycles per offset. A fatal discrepancy clears offset
validity, records fault and forcibly disarms the power stage.

The pinned, reproducible source patch is
`v0.8.11/fixed_adc_overlay.tar.xz` with SHA-256
`9f9073476f69a84938fbe2e74e73baaf764691ac252baac60d85a1e03dc22b1b`.
It contains `apply_v0811.py`, the current driver, host sanitizer test
and detailed design notes. The CI rebuilds everything starting with
the canonical original source, then applies the archive after v0.8.10,
rejects any gate-enabled compilation, checks all 512-sample offset
tests and performs the full encrypted Ninebot UART/IAP emulator regression.

**Crucial limitation:** The existing app v0.3.4 does NOT unlock E8/EF
on build `0x080B`. App BLE and NinebotCrypto have not changed.
E8/ED sector semantics are deliberately not represented as a valid SVM
sector for this fixed scan. A new explicitly versioned phone decoder
is needed for EF if/when a recoverable spare controller is bench-tested.

Physical ADC timing, current shunt polarity/gain, motor R/L/flux,
10S bus scaling, complementary TIM1 driver pins and stock bootloader
rollback are still not validated on the valuable original ESC. No
flashing or driving. ST-Link is recovery-only; phone/dashboard BLE
remains the normal diagnostic path.

The full source-only package is available as a GitHub Actions artifact
after the workflow finishes successfully. See README.md inside the
pinned overlay for the complete register description.
