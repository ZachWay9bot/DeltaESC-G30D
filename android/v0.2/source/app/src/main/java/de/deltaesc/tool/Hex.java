package de.deltaesc.tool;

import java.util.Locale;

final class Hex {
    private Hex() {}
    static String of(byte[] data) {
        if (data == null) return "";
        StringBuilder sb = new StringBuilder(data.length * 3);
        for (int i = 0; i < data.length; i++) {
            if (i > 0) sb.append(' ');
            sb.append(String.format(Locale.US, "%02X", data[i] & 0xFF));
        }
        return sb.toString();
    }
    static byte[] parse(String text) {
        String cleaned = text.replace("0x", "").replace("0X", "").replaceAll("[^0-9A-Fa-f]", "");
        if ((cleaned.length() & 1) != 0) throw new IllegalArgumentException("Ungerade Anzahl Hex-Zeichen");
        byte[] out = new byte[cleaned.length() / 2];
        for (int i = 0; i < out.length; i++) out[i] = (byte) Integer.parseInt(cleaned.substring(i * 2, i * 2 + 2), 16);
        return out;
    }
}
