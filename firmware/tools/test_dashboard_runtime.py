#!/usr/bin/env python3
from pathlib import Path
import os, subprocess
root=Path(__file__).resolve().parents[1]
out=root/'build'/'dashboard_runtime_host_test'
out.parent.mkdir(exist_ok=True)
cc=os.environ.get('HOST_CC','clang')
subprocess.run([cc,'-std=c11','-Wall','-Wextra','-Werror','-Isrc',
                'tools/dashboard_runtime_host_test.c','src/dashboard_runtime.c',
                '-o',str(out)],cwd=root,check=True)
subprocess.run([str(out)],check=True)
print('G30 dashboard runtime host test: PASS')
