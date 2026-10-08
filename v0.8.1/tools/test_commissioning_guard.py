#!/usr/bin/env python3
import os,subprocess,sys
from pathlib import Path
root=Path(sys.argv[1]).resolve()
cc=os.environ.get('HOST_CC','clang')
out=root/'build/host_commissioning_guard'
subprocess.check_call([cc,'-std=c11','-O1','-Wall','-Wextra','-Werror','-Isrc',str(Path(__file__).resolve().parent/'commissioning_guard_host_test.c'),'src/commissioning_guard.c','-o',str(out)],cwd=root)
subprocess.check_call([str(out)])
