#!/usr/bin/env python3
import os,subprocess,tempfile,pathlib
root=pathlib.Path(__file__).resolve().parents[1]
cc=os.environ.get('HOST_CC','cc')
with tempfile.TemporaryDirectory() as td:
    exe=pathlib.Path(td)/'motor_cfg_test'
    subprocess.check_call([cc,'-std=c11','-O1','-Wall','-Wextra','-Werror','-fsanitize=undefined','-I',str(root/'src'),str(root/'tools/motor_config_txn_test.c'),str(root/'src/motor_config_txn.c'),'-o',str(exe)])
    subprocess.check_call([str(exe)])
print('motor config transaction host test: PASS')
