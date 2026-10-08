#!/usr/bin/env python3
"""Host-side power button and source safety checks, not real hardware proof."""
from pathlib import Path
import os
import subprocess
import tempfile
import sys
root=Path(sys.argv[1]).resolve()
cc=os.environ.get("HOST_CC","cc")
with tempfile.TemporaryDirectory() as td:
    exe=Path(td)/"power_test"
    subprocess.check_call([cc,"-std=c11","-Wall","-Wextra","-Werror","-I",str(root/"src"),
        str(root/"tools/power_logic_host_test.c"),str(root/"src/power_logic.c"),"-o",str(exe)])
    subprocess.check_call([str(exe)])
src=(root/"src/main.c").read_text()
link=(root/"src/ninebot_link.h").read_text()
iap=(root/"src/iap_update.c").read_text()
mk=(root/"Makefile").read_text()
for needle in (
    "POWER_BUTTON_PIN 12u","POWER_HOLD_PIN 11u","GPIO_IDR(GPIOA_BASE)&(1u<<POWER_BUTTON_PIN)",
    "gpio_cfg_nibble(GPIOA_BASE, POWER_BUTTON_PIN, 0x8u)",
    "GPIO_BRR(GPIOA_BASE) = (1u << POWER_HOLD_PIN)","g_poweroff_deadline_ms",
    "iap_update_active()","case 0x10u:","case 0xD0u:","case 0x79u:",
    "POWER_OFF_ACK_DELAY_MS 150u","STOCK_COMPAT_FW_WORD 0x0420u",
    "FW_BUILD 0x0609u",
): assert needle in src,needle
assert "#define NINEBOT_WRITE     0x02u" in link
assert "#define NINEBOT_WRITE_NR  0x03u" in link
assert "#define NINEBOT_WRITE_ACK 0x05u" in link
assert "iap_control_commit" in iap and "iap_control_invalidate" in iap
assert "src/iap_control.c" in mk and "src/power_logic.c" in mk
for name in ("syncsafe","observer_zero_vector","sensorless_bench"):
    b=root/"build"/("DeltaESC_G30D_v0_6_7_ble_"+name+".bin")
    assert b.is_file() and 256<=b.stat().st_size<=0xC800,(str(b),b.stat().st_size if b.exists() else -1)
print("v0.6.9 host power logic + integration preflight: PASS")
