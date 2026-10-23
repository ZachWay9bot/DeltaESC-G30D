#!/usr/bin/env python3
import hashlib, importlib.util, json, struct, tempfile, zipfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('pack',ROOT/'tools/make_shu_zip.py'); pack=importlib.util.module_from_spec(spec); spec.loader.exec_module(pack)
plain=struct.pack('<II',0x20005000,0x08001001)+bytes(range(256))*4
enc=pack.ninebot_tea_encrypt(plain)
assert len(enc)==1040
assert enc[:16].hex()=='e442260569d928a71e88a4f51b0b75d3'
assert hashlib.md5(enc).hexdigest()=='481406633bae0c36720d6201207c44d4'
for fn,name,var in [
 ('DeltaESC_G30D_v0_6_7_ble_syncsafe.bin','DeltaESC G30D v0.6.7 SAFE','syncsafe'),
 ('DeltaESC_G30D_v0_6_7_ble_observer_zero_vector.bin','DeltaESC G30D v0.6.7 ZERO','zero-vector'),
 ('DeltaESC_G30D_v0_6_7_ble_sensorless_bench.bin','DeltaESC G30D v0.6.7 BENCH','sensorless-bench')]:
    b=(ROOT/'build'/fn).read_bytes(); pack.validate_app_image(b)
    with tempfile.TemporaryDirectory() as td:
        out=Path(td)/'x.zip'; p,e=pack.build_package(ROOT/'build'/fn,out,name,var)
        with zipfile.ZipFile(out) as z:
            assert set(z.namelist())=={'FIRM.bin.enc','info.json','params.txt'}
            assert 'FIRM.bin' not in z.namelist() and z.read('FIRM.bin.enc')==e
            info=json.loads(z.read('info.json'))['firmware']
            assert info['compatible']==['max_DRV_STM32F103CxT6'] and info['encryption']=='encrypted' and info['enforceModel'] is True
            assert info['md5']=={'enc':hashlib.md5(e).hexdigest()}
print('SHU ZIPv3/NinebotTEA preflight: PASS')
