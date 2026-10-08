# v0.6.8 staged-IAP power-loss audit overlay

**DEVELOPMENT / SOURCE ONLY. NOT A SHU FLASH RELEASE.**

Base: CI-green v0.6.7 app-compat firmware, exact original source hash validated before applying the patch.

## New audit findings

1. v0.6.7 erased the staging image before neutralizing a previously committed update-control marker. A power interruption after staging erase could leave a pending request referencing incomplete staged data.
2. v0.6.7 wrote the 0x0000505A control magic before finishing the flag/size fields. This creates an avoidable partially committed state on power interruption.

## Hardened ordering

- BEGIN: validate command/size, erase and verify old control block **before** staging erase.
- RECEIVE: retain existing 128-byte Ninebot encrypted IAP, checksum, and vector tests.
- VERIFY: check external checksum + decrypted internal checksum + application vector.
- COMMIT: write flag and size first, verify them, then upper magic halfword, then **lower magic halfword last**.
- The marker is only complete after metadata is verified.
- Power interruption simulation tests six possible cuts, including older pending markers and rejected image-size overflow.

The 50 KiB application ceiling is enforced for the committed size. This is an added conservative bound, not proof of the preserved bootloader behavior.

## Outstanding release blockers

- Confirm actual DRV126 bootloader handling of a partially programmed control block and the exact staging/copy logic against the original 128 KiB controller full dump.
- Verify that erasing the control page at BEGIN does not affect any unrelated stock bootloader/config state.
- Confirm interrupted transfer, retry, rollback and dashboard persistence on noncritical test hardware.
- Only after those conditions may a SYNC-SAFE SHU hardware candidate be proposed.

No motor run and no flash to the original 3-cap controller.
