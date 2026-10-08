# DashBLE Transaction v0.3.2 — companion for DeltaESC v0.8.4

**Android application build only; not yet tested on a physical ESC with DeltaESC 0x0804.**

## Proven transport preserved

Built from DashBLE Config 0.3.1 and its prior DashBLE Motor 0.3.0
handset/stock dashboard observations. These sources stay byte-exact:

- BleUartClient.java SHA-256 3bbf0a9e06dd277ef30cecae9baf579f8f843b0e5db505853cfe78c916b14485
- NinebotCrypto.java SHA-256 57d88389891ad2fe5dc42dc69748c2a81dc24edc3930081a74ceee45ae035705
- NinebotProtocol.java SHA-256 866fb803a439faf78ce50655cf54cae15a4467766826088e0bfa86b8468e84a3

The APK installs under a new application ID, de.deltaesc.motorparams2,
and does not replace the previously working apps.

## Strict versioned transaction

**All writes are disabled** unless NinebotCrypto authenticates, read D0
contains the DESC signature, and build is **exactly 0x0804**. This excludes
the current reported stock DRV126 0x0907 and all earlier development
builds. The five fields must be explicitly entered as externally
measured inputs: R [mOhm], L [uH], flux [mWb], phase offset [signed Q16],
test current [mA]. These are not auto-identified by the app.

After a user confirmation, the app:

1. Sends five sequential magic-protected F0..F4 WRITE 0x03 requests and
   requires a one-byte status-6 ACK for each.
2. Reads the read-only E7 status page; requires mask 0x1F and gate OFF.
3. Sends F5 + magic to commit the tuple atomically; requires status 6.
4. Reads E7 and verifies mask clear, active valid, model valid,
   gate OFF, R, L and test-current match.
5. Reads D3, D4 and D5 serially and matches active R/L/flux.
6. Stops without automatic resend or next-stage write on errors,
   disconnects or 1.8-second timeouts.

An explicit *discard staged tuple* button uses F5 + magic + 00 and
checks E7 mask=0. If the phone disconnects during staging, no F5 commit
is issued. The incomplete tuple is left staged until the user manually
aborts it after reconnection or the ESC reboots. The app never sends E5,
E6, IAP/SHU or arbitrary ESC writes.

For privacy, raw reports may contain the device name and serial. Check
before sharing them publicly.

## Safety / limitations

- Current stock firmware lacks any DESC/0x0804 identity, so writes
  are correctly locked on the user's scooter.
- v0.8.4 on the ESC is still only a software-tested SOURCE-ONLY candidate.
  Current gain/polarity, ADC TIM1 timing, MOSFET driver and R/L/flux
  measurements have not been hardware-validated.
- The 0x0804 commit and E7 protocol have been simulated on ARM in the
  firmware CI, but a real phone-to-real DeltaESC end-to-end test is absent.
- v0.3.2's five-field UI does not measure flux, R or L. Values should
  come from a separate validated measurement workflow.
- ST-Link is reserved exclusively for emergency recovery. Ordinary
  diagnosis and configuration continue over smartphone DashBLE /
  original G30 dashboard BLE / PA2 UART2.
