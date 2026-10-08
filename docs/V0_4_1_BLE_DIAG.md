# v0.4.1 — Stock BLE / NinebotCrypto diagnostic bridge

## Architecture

```
Android app
  -> BLE NUS + NinebotCrypto
Stock G30 dashboard
  -> decrypted Ninebot UART, 115200 8N1 half-duplex
PA2 / USART2 on the ESC
  -> DeltaESC private diagnostic protocol
```

NinebotCrypto is handled by the phone/dashboard side. DeltaESC sees and returns normal Ninebot UART frames:

```
5A A5 LEN SRC DST CMD ARG PAYLOAD... CK_LO CK_HI
```

DeltaESC address is `0x20`. The private command is `0x7D`.

## Commands

### HELLO — ARG 0x00

Request: empty payload.

Response payload, 8 bytes:

| Offset | Meaning |
|---:|---|
| 0 | private protocol version = 1 |
| 1 | status flags |
| 2 | firmware major = 0 |
| 3 | firmware minor = 4 |
| 4 | firmware patch = 1 |
| 5..7 | ASCII `SLE` = Sensorless / stock Link / Experimental |

Status flags:

- bit 0: sensorless branch
- bit 1: stock dashboard BLE/Ninebot diagnostics compiled in
- bit 2: build was compiled active-capable
- bit 3: bridge currently armed
- bit 4: safety latch is non-zero

The first two bytes remain compatible with the older DeltaESC Config app's HELLO detection.

### DIAG — ARG 0x20

Request: empty payload.

Response payload, 54 bytes, little-endian:

| Offset | Type | Meaning |
|---:|---|---|
| 0 | U8 | snapshot format = 1 |
| 1 | U8 | status flags |
| 2 | U16 | current residual, latest ADC counts |
| 4 | U16 | current residual peak, ADC counts |
| 6 | U32 | safety latch / fault |
| 10 | U32 | synchronized ADC sample count |
| 14 | U32 | minimum PWM/ADC sample interval, CPU cycles |
| 18 | U32 | maximum PWM/ADC sample interval, CPU cycles |
| 22 | U32 | latest 4 kHz control runtime, CPU cycles |
| 26 | U32 | maximum 4 kHz control runtime |
| 30 | U32 | latest ADC ISR runtime |
| 34 | U32 | maximum ADC ISR runtime |
| 38 | U16 | ADC raw IA |
| 40 | U16 | ADC raw IB |
| 42 | U16 | ADC raw IC |
| 44 | U16 | ADC raw VBUS |
| 46 | U32 | over-current trip count |
| 50 | U32 | hard control-overrun count |

## Safety policy

v0.4.1 is deliberately read-only over Bluetooth. No private BLE command arms the bridge, writes motor parameters, starts detection, or applies torque. The first purpose is to prove:

1. NinebotCrypto authentication on the phone;
2. stock dashboard bridge;
3. PA2 USART2 half-duplex link;
4. ADC synchronization and runtime timing on the real G30D ESC.

Only after those values are sane should a later build expose controlled power-stage actions.

## Important hardware note

PA2 is the single-wire dashboard link. PA3 remains the phase-A current ADC input. The older temporary PB6/PB7 textual debug UART was removed from the canonical v0.4.1 source.
