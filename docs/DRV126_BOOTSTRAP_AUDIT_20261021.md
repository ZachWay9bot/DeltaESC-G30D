# DRV126 bootstrap: limited binary recheck, 21 October 2026

Purpose: assess the owner's priority that stock return must remain possible
without ST-Link. No hardware, power, erase, write or reset was performed.
The private stock dump is not added to this repository.

## Exact reference and method

The saved `esc126_fulldump.bin` is 131,072 bytes. Its SHA256 is
`9235b466a2f7449b8184560dbef9012d7c12b099e4076100a223ef68d204bb68`,
matching the earlier DRV126 IAP reference and 13 October binary audit.
The first 4 KiB hash to
`c94dc4694364a664c36f55ef0109f2d078bb6f12d7427fbbfab97f7b695fd116`.
Stock boot vectors are SP `0x20000550`, Thumb reset `0x08000121`.

Only that boot region was inspected with little-endian Thumb disassembly.
Identified routine ranges were inspected separately so literal pools were
not treated as continuous executable code. This was not an exhaustive CFG,
formal proof, full decompilation or test of the currently installed loader.

## Observations

- Reset at `0x08000120` dispatches startup functions through literal-loaded
  pointers. Its presence does not establish a serial rescue listener.
- Code at `0x080005AC` selects installation/application paths from control
  state. The routines at `0x0800064C` and `0x080006D8` check staging state,
  erase the active range and copy staged data from `0x0800E800` to
  `0x08001000` in 256-byte chunks. This matches the existing pending-image
  installation role; it does not itself receive a new image over UART.
- The handoff routine at `0x08000734` checks the app stack-pointer region,
  loads the reset vector, sets MSP through a helper and branches to the app.
  A valid vector alone cannot establish that the app's later IAP loop works.
- Exact little-endian literals for USART1/2/3 peripheral bases and the
  `0x1FFFF000` system-ROM base were not found in the first 4 KiB. This is a
  narrow byte-scan result, not proof that all computed-address or conditional
  recovery mechanisms are absent.
- The candidate's existing `Default_Handler()` performs a terminal motor
  stop and loops forever; it does not service IAP. Normal E6/motor-fault
  paths are different: they return to the main loop, whose real IAP parser
  is covered by the integration tests.

## Consequence

The original install-copy loader and preserved app IAP are distinct. These
observations do **not** prove an app-independent UART entry when the app
cannot start. The reference dump's identity also does not prove the exact
loader currently installed on the target. Do not promise unconditional
rollback from these bytes and do not install a different loader based on
this limited audit.

Software improvements keep the existing IAP protocol and its source bytes
unchanged, and verify access after ordinary motor stops/faults. Before any
flash recommendation, identify the actual current firmware, accessible
transport and real stock-return procedure without relying on ST-Link.
The native tests and this binary inspection supply no physical restore
result and do not lift the existing release hold.
