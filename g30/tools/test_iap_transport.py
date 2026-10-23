#!/usr/bin/env python3
import os,subprocess,tempfile
from pathlib import Path
r=Path(__file__).resolve().parents[1]; cc=os.environ.get('HOST_CC','clang')
with tempfile.TemporaryDirectory() as d:
 o=Path(d)/'t'; subprocess.run([cc,'-std=c11','-Wall','-Wextra','-Werror','-Isrc','tools/iap_transport_host_test.c','src/iap_proto.c','-o',str(o)],cwd=r,check=True); subprocess.run([str(o)],check=True)
