package de.deltaesc.diag;

import java.util.Arrays;

public final class SescProtocol {
    public static final int CMD_CONFIG = 0x7D;
    public static final int ESC_ADDR = 0x20;
    public static final int APP_ADDR = 0x3E;

    public static final int HELLO = 0x00;
    public static final int DIAG = 0x20;

    private SescProtocol() {}

    public static byte[] buildBleInner(int src, int dst, int cmd, int arg, byte[] payload) {
        if (payload == null) payload = new byte[0];
        byte[] out = new byte[payload.length + 7];
        out[0] = 0x5A;
        out[1] = (byte)0xA5;
        out[2] = (byte)payload.length;
        out[3] = (byte)src;
        out[4] = (byte)dst;
        out[5] = (byte)cmd;
        out[6] = (byte)arg;
        System.arraycopy(payload,0,out,7,payload.length);
        return out;
    }

    public static byte[] buildConfigBleInner(int arg, byte[] payload) {
        return buildBleInner(APP_ADDR, ESC_ADDR, CMD_CONFIG, arg, payload);
    }

    public static Frame parseBleInner(byte[] raw) {
        if (raw == null || raw.length < 7 || u8(raw[0]) != 0x5A || u8(raw[1]) != 0xA5) return null;
        int len = u8(raw[2]);
        if (raw.length != len + 7) return null;
        return new Frame(u8(raw[3]),u8(raw[4]),u8(raw[5]),u8(raw[6]),
                Arrays.copyOfRange(raw,7,7+len));
    }

    public static Hello parseHello(byte[] p) {
        if (p == null || p.length < 5) return null;
        Hello h = new Hello();
        h.protocol = u8(p[0]);
        h.flags = u8(p[1]);
        h.major = u8(p[2]);
        h.minor = u8(p[3]);
        h.patch = u8(p[4]);
        return h;
    }

    public static Diag parseDiag(byte[] p) {
        if (p == null || p.length < 54) return null;
        Diag d = new Diag();
        d.format = u8(p[0]);
        d.flags = u8(p[1]);
        d.currentResidual = u16(p,2);
        d.currentPeak = u16(p,4);
        d.safetyLatch = u32(p,6);
        d.adcSamples = u32(p,10);
        d.sampleMinCycles = u32(p,14);
        d.sampleMaxCycles = u32(p,18);
        d.controlLastCycles = u32(p,22);
        d.controlMaxCycles = u32(p,26);
        d.isrLastCycles = u32(p,30);
        d.isrMaxCycles = u32(p,34);
        d.adcIa = u16(p,38);
        d.adcIb = u16(p,40);
        d.adcIc = u16(p,42);
        d.adcVbus = u16(p,44);
        d.overCurrentTrips = u32(p,46);
        d.hardOverruns = u32(p,50);
        return d;
    }

    public static int u8(byte b) {
        return b & 0xFF;
    }

    public static int u16(byte[] b, int o) {
        return u8(b[o]) | (u8(b[o+1]) << 8);
    }

    public static long u32(byte[] b, int o) {
        return ((long)u8(b[o])) |
                ((long)u8(b[o+1]) << 8) |
                ((long)u8(b[o+2]) << 16) |
                ((long)u8(b[o+3]) << 24);
    }

    public static final class Frame {
        public final int src;
        public final int dst;
        public final int cmd;
        public final int arg;
        public final byte[] payload;

        public Frame(int src, int dst, int cmd, int arg, byte[] payload) {
            this.src = src;
            this.dst = dst;
            this.cmd = cmd;
            this.arg = arg;
            this.payload = payload;
        }
    }

    public static final class Hello {
        public int protocol, flags, major, minor, patch;

        public boolean sensorless() { return (flags & 0x01) != 0; }
        public boolean bleDiag() { return (flags & 0x02) != 0; }
        public boolean activeCapable() { return (flags & 0x04) != 0; }
        public boolean armed() { return (flags & 0x08) != 0; }
        public boolean faulted() { return (flags & 0x10) != 0; }
    }

    public static final class Diag {
        public int format, flags;
        public int currentResidual, currentPeak;
        public long safetyLatch, adcSamples;
        public long sampleMinCycles, sampleMaxCycles;
        public long controlLastCycles, controlMaxCycles;
        public long isrLastCycles, isrMaxCycles;
        public int adcIa, adcIb, adcIc, adcVbus;
        public long overCurrentTrips, hardOverruns;

        public boolean armed() { return (flags & 0x08) != 0; }
        public boolean faulted() { return (flags & 0x10) != 0; }
    }
}
