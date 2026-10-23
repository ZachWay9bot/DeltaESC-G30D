#!/usr/bin/env python3
import os, subprocess, tempfile
from pathlib import Path
r=Path(__file__).resolve().parents[1]
cc=os.environ.get("HOST_CC","clang")
with tempfile.TemporaryDirectory() as d:
    exe=Path(d)/"iap_control_powercut"
    subprocess.run([cc,"-std=c11","-Wall","-Wextra","-Werror","-Isrc",
        "src/iap_control.c","tools/iap_control_powercut_test.c","-o",str(exe)],cwd=r,check=True)
    subprocess.run([str(exe)],check=True)
s=(r/"src/iap_update.c").read_text()
assert s.index("iap_control_invalidate(flash_erase_page,control_read_word)") < s.index("if(!erase_stage())")
assert "return iap_control_commit(size,flash_half,control_read_word)" in s
print("IAP control/staging order: PASS")
