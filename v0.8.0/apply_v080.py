#!/usr/bin/env python3
"""Import complete v0.7.0 parameterized motor control into v0.7.2 safe BLE firmware.

Proven NinebotCrypto/PA2, power-button and IAP sources are not replaced.
Motor pins stay alternate-function DISABLED in the only allowed build.
"""
from pathlib import Path
import hashlib, tarfile, sys
root=Path(sys.argv[1]).resolve()
overlay=Path(sys.argv[2]).resolve()
expect={
 "sensorless_control.c":"3307e0543ec3b49c4ef34f3b4f014e0d2329b20e847b91142a8078a27f2286cd",
 "sensorless_control.h":"a49227a32222f27ced52ca8eb370728896dc1a541cead3b04c6a3f6f02daf706"
}
with tarfile.open(overlay,"r:xz") as t:
    for name,sha in expect.items():
        matched=[m for m in t.getmembers() if m.isfile() and m.name.lstrip("./")=="src/"+name]
        if len(matched)!=1:raise RuntimeError("motor overlay is not the pinned audited source: "+name)
        raw=t.extractfile(matched[0]).read()
        if hashlib.sha256(raw).hexdigest()!=sha:raise RuntimeError("wrong motor overlay hash: "+name)
        (root/"src"/name).write_bytes(raw)
# Fix two pre-existing undefined signed-left-shifts in the v0.7.0 BEMF
# calculation. UBSan found the negative back-EMF case in motor host tests.
mc=root/"src/sensorless_control.c"
ms=mc.read_text()
for expr in ("va_mv - ra_mv - la_mv","vb_mv - rb_mv - lb_mv"):
    old=f"({expr}) << 8"
    new=f"(int32_t)((int64_t)({expr}) * 256LL)"
    if ms.count(old)!=1:raise RuntimeError("expected BEMF shift missing: "+expr)
    ms=ms.replace(old,new)
mc.write_text(ms)
def sub1(s,old,new):
    n=s.count(old)
    if n!=1:raise RuntimeError("replace marker count "+str(n)+": "+old[:140])
    return s.replace(old,new,1)
p=root/"src/main.c";s=p.read_text()
s=sub1(s,"#define FW_BUILD 0x0702u",'''#define FW_BUILD 0x0800u
#if MOTOR_VOLTAGE_BENCH || SENSORLESS_CLOSED_LOOP_ALLOWED
#error "v0.8.0 no active motor variants: software integration only"
#endif''')
s=sub1(s,"v0.7.2 BLE motor telemetry must NEVER arm a physical power stage",
         "v0.8.0 integrated motor candidate must NEVER arm a physical power stage")
s=sub1(s,"ctrl.flux_sq","ctrl.bemf_mv")
s=sub1(s,"v0.7.2 DASHBOARD BLE MOTOR TELEMETRY",
          "v0.8.0 DASHBLE PARAMETERIZED MOTOR CORE GATES OFF")
marker="static void app_write_config(const ninebot_frame_t *f){"
helper='''/* Unlike the v0.7.2 placeholder fields, these F0/F1/F2 values genuinely
 * configure the R/L/flux observer. Incomplete tuples never unlock observer.
 * Physical ADC scale, sign, polarity and model validity are not verified. */
static void apply_motor_params_if_complete(void) {
    if (g_cfg_r_uohm && g_cfg_l_nh && g_cfg_flux_uwb)
        (void)sensorless_control_set_motor_params(&ctrl,
            g_cfg_r_uohm,g_cfg_l_nh,g_cfg_flux_uwb);
}

'''
s=sub1(s,marker,helper+marker)
s=sub1(s,marker.replace("static", "static") if False else
       "    uint8_t cmd=f->arg;\n    if(cmd==0xF0u)",
       "    uint8_t cmd=f->arg;\n    if(g_power_armed){action_ack(f,cmd,ACT_UNSAFE);return;}\n    if(cmd==0xF0u)")
for field in ("g_cfg_r_uohm","g_cfg_l_nh","g_cfg_flux_uwb"):
    s=sub1(s,field+"=v;g_cfg_dirty=1u;action_ack(f,cmd,ACT_OK);return;",
       field+"=v;g_cfg_dirty=1u;apply_motor_params_if_complete();action_ack(f,cmd,ACT_OK);return;")
# All hardware PWMs remain disabled despite the motor observer being compiled in.
assert '#if POWER_STAGE_ARM_ALLOWED || SENSORLESS_RUN_ALLOWED' in s
assert '#error "v0.8.0 integrated motor candidate must NEVER arm a physical power stage"' in s
assert 'case 0xDAu: case 0xDBu: case 0xDCu: case 0xDDu: case 0xDEu: case 0xDFu:' in s
p.write_text(s)
m=root/"Makefile";mstr=m.read_text()
assert "all: safe\n" in mstr and "active: " not in mstr and "bench: " not in mstr
assert "-DPOWER_STAGE_ARM_ALLOWED=0 -DSENSORLESS_RUN_ALLOWED=0" in mstr
m.write_text(mstr.replace("v0_7_2","v0_8_0"))
print("v0.8.0: parameterized back-EMF motor core + DA-DF DashBLE, hardware gates off")
