#!/usr/bin/env python3
from pathlib import Path
import subprocess, tempfile, struct, sys

tool=Path(__file__).with_name('audit_drv126_fulldump.py')
with tempfile.TemporaryDirectory() as td:
    p=Path(td)/'fake.bin'
    b=bytearray([0xFF])*(128*1024)
    struct.pack_into('<II',b,0,0x20000550,0x08000121)
    struct.pack_into('<II',b,0x1000,0x20002B18,0x08001101)
    # one aligned/halfword-searchable TIM1 literal
    struct.pack_into('<I',b,0x2000,0x40012C00)
    p.write_bytes(b)
    out=subprocess.check_output([sys.executable,str(tool),str(p)],text=True)
    assert 'boot_vector_valid=1' in out
    assert 'app_vector_valid=1' in out
    assert 'literal_TIM1=1' in out
    assert 'd800_dfff_all_ff=1' in out
print('PASS: DRV126 dump structural audit fixture')
