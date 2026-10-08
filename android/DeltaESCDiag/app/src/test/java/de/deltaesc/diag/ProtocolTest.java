package de.deltaesc.diag;

import org.junit.Test;
import static org.junit.Assert.*;

public class ProtocolTest {
    @Test public void configInnerFrameHasExpectedHeader() {
        byte[] f = SescProtocol.buildConfigBleInner(SescProtocol.HELLO,new byte[0]);
        assertEquals(7,f.length);
        assertEquals(0x5A,SescProtocol.u8(f[0]));
        assertEquals(0xA5,SescProtocol.u8(f[1]));
        assertEquals(SescProtocol.APP_ADDR,SescProtocol.u8(f[3]));
        assertEquals(SescProtocol.ESC_ADDR,SescProtocol.u8(f[4]));
        assertEquals(SescProtocol.CMD_CONFIG,SescProtocol.u8(f[5]));
        assertEquals(SescProtocol.HELLO,SescProtocol.u8(f[6]));
    }

    @Test public void parsesDiagnosticSnapshot() {
        byte[] p = new byte[54];
        p[0] = 1;
        p[1] = 3;
        p[2] = 12;
        p[4] = 34;
        p[10] = 0x78;
        p[11] = 0x56;
        p[12] = 0x34;
        p[13] = 0x12;
        SescProtocol.Diag d = SescProtocol.parseDiag(p);
        assertNotNull(d);
        assertEquals(12,d.currentResidual);
        assertEquals(34,d.currentPeak);
        assertEquals(0x12345678L,d.adcSamples);
    }
}
