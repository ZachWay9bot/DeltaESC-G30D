# DeltaESC Android v0.2 PRE-FLIGHT protocol

Logical plaintext before NinebotCrypto:

`5A A5 LEN SRC DST CMD ARG [payload...]`

- phone SRC 0x3E
- stock dashboard DST 0x21 for 5B/5C/5D login
- ESC DST 0x20
- register read CMD 0x01
- register write CMD 0x02
- read ACK CMD 0x04

The BLE encrypted transport adds NinebotCrypto authentication/counter data. Received frames are accepted only after MIC verification.

## v0.2 safety policy

ESC CMD 0x02 is unconditionally rejected by the BLE client. v0.2 is a read-only pre-flash validator.

## DeltaESC identity

Read D0 requesting 16 bytes. DeltaESC v0.6.3 must reply:
- 0..3 ASCII DESC
- 4 protocol major
- 5 protocol minor
- 6..7 build LE = 0x0603
- 8 state
- 9 flags
- 10..11 fault
- 12..13 PWM Hz
- 14..15 control Hz

A stock D0 reply is explicitly not accepted as DeltaESC identity.
