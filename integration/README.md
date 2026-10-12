# DeltaESC v0.9.19 – Motor-RC + read-only ADC proof integration

**EXPERIMENTAL SOURCE / NO FLASH / NO MOTOR AUTHORIZATION**

This independent integration branch is based on the CI-green PR #6 Motor-RC
commit `2ce5833cb6838518affdd594ba2c13417e59fedb`.
It carries the exact read-only ADC proof header/unit test from PR #5
(commit `59607caf3f7b940732e49f7578d2ec00b3e625b8`).

Reconstruction in `.github/workflows/motor-rc-adc-v119.yml`:

1. Build and test all 26 original v0.9.14 Motor-ID groups.
2. Apply v0.9.16 BDTR and v0.9.17 terminal MOE kill overlays.
3. Apply PR #6 coast-vs-E6 experimental E7-only patch.
4. Run the v0.9.18 ADC proof unit tests.
5. Apply `integration/tools/apply_v119.py`, adding **READ-only FA/FB** snapshots
   without changing E7 drive or E6/stop logic.
6. Run v0.9.19 contracts and compile **safe** and **E7-only experimental**
   STM32F103 binaries. Compile must still reject normal driving.

**Build identity is 0x0919**, deliberately different from 0x0914 and
0x0918. The existing Android v0.3.6 Motor-ID app is locked to 0x0914
and is **not compatible**. FA/FB phone support and guard for 0x0919
must be implemented/tested separately before considering any BLE bench use.

The original physical G30D still lacks independent proof of MT8006A
input/output isolation, AMP_LIM response, current gain, calibrated ADC
timing, and power-latch routing. CI PASS does not mean hardware safety.
Do not flash either binary to the original controller.
