# DeltaESC Link Probe v0.2.3 — real G30 framing

Read-only Android link probe for the stock Ninebot G30D dashboard.

This revision fixes the protocol-layer mismatch that caused the previous v0.2 line to fail even though GATT/NUS transport was available.

## Physical reference

The regression vectors come from the hardware-proven G30 Bench BLE 0.1.0 path on `NBScooter2088`.

The BLE/NinebotCrypto plaintext is:

`5A A5 LEN SRC DST CMD ARG PAYLOAD...`

It is exactly `LEN + 7` bytes. There is **no classic UART checksum inside this encrypted BLE frame**. NinebotCrypto adds six trailer bytes, therefore the encrypted NUS wire frame is `LEN + 13` bytes.

## v0.2.3 fixes

- removes the erroneous two-byte UART checksum from the BLE crypto-inner frame;
- encrypted RX reassembly uses `LEN + 13`, not `LEN + 15`;
- initial 5B response accepts the real 37-byte plaintext / 43-byte wire length;
- 13-byte 5C/5D replies are accepted;
- checks GATT queue availability before encryption so an unsent frame cannot advance the crypto counter;
- retry counters count only frames actually accepted by the GATT queue;
- read-only probe requests are retried if the single-frame GATT queue is temporarily busy.

## Mandatory real-hardware regression vectors

CI verifies captured bytes for:
- initial 5B TX and 5B RX;
- 5C TX at counters 2 and 3;
- 5C00 RX;
- 5C01 session-key switch;
- 5D TX at counter 23 and 5D01 RX;
- 20/20/3 NUS notification reassembly.

## Safety boundary

The app only performs dashboard authentication and read-only ESC reads for `0x1A`, `0x10` and `0xD0`.

It contains no motor command, no E/F tuning writes, no IAP command and no firmware flashing path.
