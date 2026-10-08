# DeltaESC G30D v0.6.5 — IAP128 + VTOR hardening

**UNVALIDATED ON REAL HARDWARE. Do not flash until this branch is CI-green and the first-flash checklist is re-audited.**

v0.6.5 is based on the CI-green/frozen v0.6.4 stock-IAP-ACK candidate.

Changes:
- accept stock Ninebot 128-byte IAP write payloads;
- accept only zero padding beyond the advertised final firmware length;
- test 8-bit IAP block-index wrap 0xFF -> 0x00;
- keep stock 0x0B ACK framing from v0.6.4;
- explicitly set SCB_VTOR to 0x08001000 at application reset;
- expand the PA2 RX ring to 256 bytes;
- quarantine IAP traffic from the normal app-frame parser while an update owns the link;
- update SHU package notes/version strings to v0.6.5.

The first real candidate remains SYNC-SAFE only. Zero-vector and sensorless-bench packages are build artifacts, not first-flash recommendations.
