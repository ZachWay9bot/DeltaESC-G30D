#!/usr/bin/env python3
import os,subprocess,sys
from pathlib import Path
root=Path(sys.argv[1]).resolve(); cc=os.environ.get('HOST_CC','clang')
out=root/'build/host_stock_current_frontend'
subprocess.check_call([cc,'-std=c11','-O1','-Wall','-Wextra','-Werror','-fsanitize=undefined','-fno-sanitize-recover=all','-Isrc','tools/stock_current_frontend_host_test.c','src/stock_current_frontend.c','-o',str(out)],cwd=root)
subprocess.check_call([str(out)])
