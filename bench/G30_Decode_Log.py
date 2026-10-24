#!/usr/bin/env python3
"""Decode the RX bytes of ONE G30_OnePass_UART.ps1 capture, completely offline.

No COM port or GPIO/flash access. Unknown frames and partial responses are
reported, not silently interpreted as verified hardware behaviour.
"""
import argparse
import hashlib
import json
import re
from pathlib import Path

RX_HEX = re.compile(r"\bRX\s+((?:[0-9a-fA-F]{2}-)*[0-9a-fA-F]{2})\s*$")
GUARD = {
    0: "ADC stale", 1: "current not idle", 2: "ADC offsets invalid",
    3: "motor parameters invalid", 4: "dashboard input stale",
    5: "throttle not idle", 6: "brake not idle", 7: "safety latched",
    8: "IAP in progress", 9: "poweroff requested",
    10: "control not stopped", 11: "VBUS ADC outside limits",
    12: "physical current scaling uncalibrated", 13: "gate hardware not qualified",
    14: "ADC sampling timing uncalibrated",
}
STOP_REASON = {
    0:"none",1:"throttle neutral",2:"brake -> coast",3:"dashboard lost",
    4:"ADC",5:"FOC fault",6:"PWM",7:"preconditions",
}
def u16(b, offset):
    return int.from_bytes(b[offset:offset+2], "little")
def i16(b, offset):
    return int.from_bytes(b[offset:offset+2], "little", signed=True)
def u32(b, offset):
    return int.from_bytes(b[offset:offset+4], "little")
def hexstr(b):
    return b.hex("-").upper()

def parse_stream(stream):
    """Resynchronising parser. Checksum identical to Ninebot 5A A5 firmware."""
    items = []
    invalid = 0
    i = 0
    while i + 9 <= len(stream):
        if stream[i:i+2] != b"\x5a\xa5":
            i += 1
            continue
        length = stream[i+2]
        total = length+9
        if length > 56 or i+total > len(stream):
            i += 1
            continue
        raw = stream[i:i+total]
        crc = (~sum(raw[2:-2])) & 0xffff
        if u16(raw,total-2) != crc:
            invalid += 1
            i += 1
            continue
        items.append({
            "source": raw[3], "destination": raw[4], "command": raw[5],
            "argument": raw[6], "payload": bytes(raw[7:-2]), "raw": raw,
        })
        i += total
    return items, invalid

def parse_log(text):
    buf = bytearray()
    for line in text.splitlines():
        m = RX_HEX.search(line)
        if m:
            buf.extend(bytes.fromhex(m.group(1).replace("-", "")))
    return parse_stream(bytes(buf)), bytes(buf)

def decode(f):
    p = f["payload"]
    arg = f["argument"]
    out = {
        "command": f"{f['command']:02X}", "register": f"{arg:02X}",
        "src": f"{f['source']:02X}", "dst": f"{f['destination']:02X}",
        "payload_hex": hexstr(p), "frame_hex": hexstr(f["raw"]),
    }
    if f["command"] != 0x04 or f["source"] != 0x20:
        out["note"] = "Not a DeltaESC read ACK. No app-specific decoding."
        return out
    if arg == 0x10 and p:
        out.update({"identity_ascii": p.decode("ascii","replace").rstrip("\0")})
    elif arg == 0xD0 and len(p)>=16 and p[:4] == b"DESC":
        out.update({
            "firmware":"DeltaESC", "build":f"{u16(p,6):04X}",
            "protocol":f"{p[4]}.{p[5]}", "foc_state":p[8],
            "status_flags":p[9],"fault":u16(p,10),
            "pwm_hz":u16(p,12),"control_hz":u16(p,14)
        })
    elif arg == 0xD1 and len(p)>=14:
        out.update({"offset_adc1":[u16(p,j) for j in (0,2,4)],
                    "raw_adc":[u16(p,j) for j in (6,8,10,12)]})
    elif arg == 0xD2 and len(p)>=5:
        out.update({"phase_map":list(p[:3]),"phase_signs":p[3],"direction":p[4]})
    elif arg in (0xD3,0xD4,0xD5) and len(p)>=4:
        out[{0xD3:"r_uohm",0xD4:"l_nh",0xD5:"flux_uwb"}[arg]]=u32(p,0)
    elif arg == 0xD7 and len(p)>=8:
        out.update({"foc_state":p[0],"observer_valid":bool(p[1]),
                    "lock_ticks":u16(p,2),"lost_ticks":u16(p,4),
                    "observer_confidence_permille":u16(p,6)})
    elif arg == 0xD9 and len(p)>=16:
        mask=u16(p,6)
        out.update({"fault":u16(p,0),"diagnostic_flags":u16(p,2),
                    "hard_current_limit_raw_counts":u16(p,4),
                    "start_blockers":[name for bit,name in GUARD.items() if mask&(1<<bit)],
                    "start_blockers_mask":mask,"adc_offsets_valid":bool(p[8]),
                    "motor_params_valid":bool(p[9]),"phase_map_valid":bool(p[10]),
                    "vbus_adc_raw":u16(p,12),"motor_nominal_mA":u16(p,14)})
    elif arg == 0xE8 and len(p)>=16:
        out.update({"adc1_injected_raw":u16(p,0),"adc2_injected_raw":u16(p,2),
                    "vbus_adc_raw":u16(p,4),"vbus_age_ms":u16(p,6),
                    "last_adc_sector":p[8],"adc_channels":list(p[9:11]),
                    "adc_flags":p[11],"vbus_timeout_events":u16(p,12)})
    elif arg == 0xFC and len(p)>=28 and p[:4] == b"RK10":
        out.update({"rolling_ready":bool(p[4]&1),"forward_direction":bool(p[4]&2),
                    "rolling_speed_kmh":u16(p,6)/10,
                    "bemf_span_raw":u16(p,8),"forward_edges":u16(p,10),
                    "reverse_edges":u16(p,12),"bad_jumps":u16(p,14),
                    "flux_phase_u16":u16(p,16),"electrical_step":i16(p,18),
                    "phase_adc_raw":[u16(p,j) for j in (20,22,24)],
                    "edge_age_ms":u16(p,26)})
    elif arg == 0xFD and len(p)>=28 and p[:4] == b"RC12":
        reason=p[7]
        out.update({"foc_state":p[4],"foc_fault":p[5],"gates_armed":bool(p[6]),
                    "stop_reason":STOP_REASON.get(reason,f"unknown {reason}"),
                    "stop_reason_code":reason,
                    "observer_phase":u16(p,8),"control_phase":u16(p,10),
                    "phase_error":i16(p,12),"electrical_step":i16(p,14),
                    "id_raw_counts":i16(p,16),"iq_raw_counts":i16(p,18),
                    "rolling_good_ticks":u16(p,20),"rolling_iq_ticks":u16(p,22),
                    "flux_clamps":u16(p,24),"last_abs_current_raw":u16(p,26)})
    elif arg == 0xFE and len(p)>=28 and p[:4] == b"PI12":
        out.update({"phase_currents_raw_counts":[i16(p,j) for j in (4,6,8)],
                    "peak_phase_counts":u16(p,10),"pwm_ccr":[u16(p,j) for j in (12,14,16)],
                    "iq_demand_counts":i16(p,18),"last_sector":p[20],
                    "next_adc_sector":p[21],"vbus_adc_raw":u16(p,22),
                    "safety_latched":u16(p,24),"pipeline_fault_latched":bool(p[26]),
                    "emergency_gate_kill_latched":bool(p[27])})
    elif arg == 0xFF and len(p)>=12 and p[:4] == b"PF13":
        out.update({"pole_pairs":p[4],"forward_raw_step":p[5],
                    "circumference_mm":u16(p,6),"phase_offset":i16(p,8),
                    "motor_params_valid":bool(p[10]),"drive_build":bool(p[11])})
    else:
        out["note"] = "Unknown or truncated read payload; kept as raw hex."
    return out

def make_report(logfile):
    content = Path(logfile).read_bytes()
    (frames, invalid), rx = parse_log(content.decode("utf-8", "replace"))
    records = [decode(f) for f in frames]
    by_register = {}
    for record in records:
        if record["command"]=="04":
            by_register.setdefault(record["register"],[]).append(record)
    return {
        "type":"G30 one-pass UART offline decode",
        "log_path":str(logfile),"log_sha256":hashlib.sha256(content).hexdigest(),
        "rx_bytes":len(rx),"valid_frames":len(frames),
        "checksum_failures_encountered":invalid,
        "read_ack_registers":by_register,
        "other_frames":[rec for rec in records if rec["command"]!="04"],
        "caveat":"Snapshot only. No motor, IAP, flash, FET or hardware validation implied."
    }

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--log",type=Path,required=True)
    parser.add_argument("--output",type=Path)
    args=parser.parse_args()
    report=make_report(args.log)
    encoded=json.dumps(report,indent=2,ensure_ascii=False)+"\n"
    if args.output:args.output.write_text(encoded,encoding="utf-8")
    print(encoded,end="")
    if not report["valid_frames"]:
        print("No valid RX frames in capture: verify COM/half-duplex and real device identity.",file=__import__("sys").stderr)
        return 2
    return 0

if __name__=="__main__":
    raise SystemExit(main())
