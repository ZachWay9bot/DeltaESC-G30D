# Android BLE recovery v0.1.2R

Purpose: recover the real G30D Gen1 controller from DeltaESC v0.6.5 back to the verified original DRV126 **over the phone BLE path only**.

The app is based directly on the hardware-proven G30 Bench BLE v0.1.1.

Hard boundaries:
- no ST-Link path;
- LegacyCrypto.java, Packet.java and WireDecoder.java remain byte-identical to v0.1.1;
- recovery unlocks only after D0 reports DESC + build 0x0605;
- embedded FIRM.bin.enc is the exact verified stock DRV126 payload;
- IAP sequence is 07 BEGIN -> 235 x 08 WRITE -> 09 CRC/COMMIT -> 0A RESET;
- expected ESC IAP response is 0x0B with result in ARG;
- no blind WRITE retries;
- no motor-control commands.

Why this exists: SHU package selection can be bypassed for max_drv_unknown, but the real v0.6.5 controller times out during SHU initialization. Public Ninebot IAP code shows three 0x02 lock writes to register 0x70 before BEGIN. v0.6.5 incorrectly treats 0x03 as the normal write opcode, so the SHU pre-IAP phase is a strong mismatch candidate. This recovery app bypasses that SHU-specific preparation and talks directly to the v0.6.5 IAP parser using the already proven phone/dashboard crypto transport.

This remains a recovery candidate until its Android CI build and the real BEGIN/ACK exchange are observed.
