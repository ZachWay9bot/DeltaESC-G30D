#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
cc -std=c11 -O2 -Wall -Wextra -Werror -Isrc src/bemf_detector.c tools/bemf_detector_host_test.c -o /tmp/xbot_bemf_test
/tmp/xbot_bemf_test
grep -Fq 'AFIO_MAPR|=AFIO_MAPR_USART1_REMAP' src/uart.c
grep -Fq 'gpio_cfg(GPIOB_BASE,1,0x0u)' src/main.c
! grep -R --line-number -E 'TIM_BDTR\([^)]*\)[[:space:]]*=' src
! grep -R --line-number -E 'TIM_CCER\([^)]*\)[[:space:]]*=' src
grep -Fq 'RCC_APB2ENR&=~(1u<<11)' src/main.c
! grep -R --line-number '0x0801FC00' src linker.ld
grep -Fq '#define STAGING_BASE   0x0800E800u' src/iap.h
grep -Fq '#define STAGING_LIMIT  0x0801C000u' src/iap.h
grep -Fq '#define UPDATE_CONTROL 0x0801F800u' src/iap.h
python3 - <<'PY2'
import sys
sys.path.insert(0,'tools')
import ninebot_pack as n
p=bytes(range(8)); c=n.crypt(p,False)
assert c.hex()=='f3d94c15a303eee0'
assert n.crypt(c,True)==p
print('PASS NinebotTEA fixed test vector')
PY2
if [[ -n "${KNOWN_ENC:-}" ]]; then
  python3 - "$KNOWN_ENC" <<'PY'
import sys
sys.path.insert(0,'tools')
import ninebot_pack as n
enc=open(sys.argv[1],'rb').read(); p=n.crypt(enc,True)
assert n.validate_plain(p)
assert n.crypt(p,False)==enc
assert p[-4:].hex()=='93d8f7e1'
print('PASS NinebotTEA golden decrypt/re-encrypt')
PY
fi
python3 tools/ninebot_pack.py verify build/ESC2_XBOT_SL02_v0.2_Driver_FIRM.bin.enc
python3 - <<'PY'
import struct
b=open('build/XBOT_SENSORLESS_PASSIVE_IAP_v0.2.bin','rb').read()
sp,pc=struct.unpack_from('<II',b)
assert 0x20000000 <= sp <= 0x20005000
assert pc&1 and 0x08001000 <= (pc&~1) < 0x0800E800
assert len(b)<0xD7FC
print('PASS vector/app-size contract',hex(sp),hex(pc),len(b))
PY
echo 'PASS passive output + flash-layout invariants'
