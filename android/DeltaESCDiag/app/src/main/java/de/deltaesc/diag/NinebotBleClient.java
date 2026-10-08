package de.deltaesc.diag;

import android.annotation.SuppressLint;
import android.bluetooth.*;
import android.bluetooth.le.*;
import android.content.Context;
import android.os.Handler;
import android.os.Looper;

import java.io.ByteArrayOutputStream;
import java.security.SecureRandom;
import java.util.*;

public final class NinebotBleClient {
    public interface Listener {
        void onStatus(String text);
        void onReady(boolean encrypted);
        void onFrame(SescProtocol.Frame frame);
        void onDisconnected();
    }

    private static final UUID NUS_SERVICE = UUID.fromString("6e400001-b5a3-f393-e0a9-e50e24dcca9e");
    private static final UUID NUS_TX = UUID.fromString("6e400002-b5a3-f393-e0a9-e50e24dcca9e");
    private static final UUID NUS_RX = UUID.fromString("6e400003-b5a3-f393-e0a9-e50e24dcca9e");
    private static final UUID CCCD = UUID.fromString("00002902-0000-1000-8000-00805f9b34fb");

    private final Context context;
    private final Listener listener;
    private final Handler handler = new Handler(Looper.getMainLooper());
    private final BluetoothAdapter adapter;
    private BluetoothLeScanner scanner;
    private BluetoothGatt gatt;
    private BluetoothGattCharacteristic tx;
    private BluetoothGattCharacteristic rx;

    private LegacyNinebotCrypto crypto;
    private boolean ready;
    private int pairState;
    private byte[] serial = new byte[14];
    private byte[] appRandom = new byte[16];

    private final ArrayDeque<byte[]> writeQueue = new ArrayDeque<>();
    private boolean writeBusy;
    private final ByteArrayOutputStream rxBuffer = new ByteArrayOutputStream();

    private static final int PAIR_WAIT_5B = 1;
    private static final int PAIR_WAIT_5C = 2;
    private static final int PAIR_WAIT_5D = 3;
    private static final int PAIR_READY = 4;

    public NinebotBleClient(Context context, Listener listener) {
        this.context = context.getApplicationContext();
        this.listener = listener;
        BluetoothManager bm = (BluetoothManager) context.getSystemService(Context.BLUETOOTH_SERVICE);
        adapter = bm.getAdapter();
    }

    @SuppressLint("MissingPermission")
    public void scanAndConnect() {
        disconnect();
        if (adapter == null || !adapter.isEnabled()) {
            listener.onStatus("Bluetooth ist ausgeschaltet");
            return;
        }
        scanner = adapter.getBluetoothLeScanner();
        listener.onStatus("Suche Ninebot…");
        scanner.startScan(scanCallback);
    }

    private void sendEncryptedPair(int cmd,int arg,byte[] payload) {
        byte[] p = SescProtocol.buildBleInner(0x3E,0x21,cmd,arg,payload);
        queueBytes(crypto.encrypt(p));
    }

    private void schedule5c() {
        handler.postDelayed(() -> {
            if (!ready && pairState == PAIR_WAIT_5C) {
                sendEncryptedPair(0x5C,0,appRandom);
                schedule5c();
            }
        },1000);
    }

    private void onRxChunk(byte[] chunk) {
        if (chunk == null || chunk.length == 0) return;
        synchronized (rxBuffer) {
            rxBuffer.write(chunk,0,chunk.length);
            while (true) {
                byte[] b = rxBuffer.toByteArray();
                int start = findHeader(b);
                if (start < 0) {
                    if (b.length > 2) rxBuffer.reset();
                    return;
                }
                if (start > 0) {
                    rxBuffer.reset();
                    rxBuffer.write(b,start,b.length-start);
                    b = rxBuffer.toByteArray();
                }
                if (b.length < 3) return;
                int len = b[2] & 0xFF;
                // NinebotCrypto wire overhead is 13 bytes total over payload:
                // 5A A5 LEN + encrypted inner + 4-byte MIC/CRC + 2-byte counter.
                int total = len + 13;
                if (total < 9 || total > 270) {
                    rxBuffer.reset();
                    return;
                }
                if (b.length < total) return;

                byte[] msg = Arrays.copyOfRange(b,0,total);
                byte[] remain = Arrays.copyOfRange(b,total,b.length);
                rxBuffer.reset();
                rxBuffer.write(remain,0,remain.length);
                handleMessage(msg);
            }
        }
    }

    private static int findHeader(byte[] b) {
        for (int i=0;i+1<b.length;i++) if ((b[i]&0xFF)==0x5A && (b[i+1]&0xFF)==0xA5) return i;
        return -1;
    }

    private void handleMessage(byte[] wire) {
        byte[] plain = crypto.decrypt(wire);
        SescProtocol.Frame f = SescProtocol.parseBleInner(plain);
        if (f == null) return;

        if (!ready && f.src == 0x21 && f.dst == 0x3E) {
            if (f.cmd == 0x5B && pairState == PAIR_WAIT_5B) {
                if (f.payload.length >= 30) {
                    serial = Arrays.copyOfRange(f.payload,16,30);
                    new SecureRandom().nextBytes(appRandom);
                    crypto.setAppRandom(appRandom);
                    pairState = PAIR_WAIT_5C;
                    listener.onStatus("Power-Taste am Scooter drücken…");
                    sendEncryptedPair(0x5C,0,appRandom);
                    schedule5c();
                }
                return;
            }
            if (f.cmd == 0x5C && pairState == PAIR_WAIT_5C) {
                if (f.arg == 1) {
                    pairState = PAIR_WAIT_5D;
                    listener.onStatus("Pairing bestätigt…");
                    sendEncryptedPair(0x5D,0,serial);
                }
                return;
            }
            if (f.cmd == 0x5D && pairState == PAIR_WAIT_5D && f.arg == 1) {
                pairState = PAIR_READY;
                ready = true;
                listener.onStatus("NinebotCrypto bereit");
                listener.onReady(true);
                return;
            }
        }

        if (ready) listener.onFrame(f);
    }

    private void queueBytes(byte[] data) {
        synchronized (writeQueue) {
            for (int p=0;p<data.length;p+=20) {
                writeQueue.add(Arrays.copyOfRange(data,p,Math.min(data.length,p+20)));
            }
        }
        writeNext();
    }

    @SuppressLint("MissingPermission")
    private void writeNext() {
        BluetoothGatt g = gatt;
        BluetoothGattCharacteristic c = tx;
        if (g == null || c == null) return;
        byte[] next;
        synchronized (writeQueue) {
            if (writeBusy) return;
            next = writeQueue.poll();
            if (next == null) return;
            writeBusy = true;
        }
        c.setWriteType(BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT);
        c.setValue(next);
        if (!g.writeCharacteristic(c)) {
            synchronized (writeQueue) { writeBusy = false; }
            handler.postDelayed(this::writeNext,30);
        }
    }
}
