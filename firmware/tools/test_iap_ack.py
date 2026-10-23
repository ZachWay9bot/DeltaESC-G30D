#!/usr/bin/env python3
import os, subprocess, tempfile
root=os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
cc=os.environ.get('HOST_CC','clang')
out=os.path.join(tempfile.gettempdir(),'deltaesc_iap_ack_test')
subprocess.check_call([cc,'-std=c11','-Wall','-Wextra','-Werror','-I'+os.path.join(root,'src'),os.path.join(root,'src','iap_proto.c'),os.path.join(root,'tools','iap_ack_host_test.c'),'-o',out])
subprocess.check_call([out])
