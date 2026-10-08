package de.deltaesc.diag;

import android.Manifest;
import android.app.Activity;
import android.content.pm.PackageManager;
import android.graphics.Typeface;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;

import java.util.Locale;

public final class MainActivity extends Activity implements NinebotBleClient.Listener {
    private static final int REQ_BLE = 10;

    private final Handler handler = new Handler(Looper.getMainLooper());
    private NinebotBleClient ble;
    private TextView status;
    private TextView identity;
    private TextView data;
    private Button connect;
    private boolean deltaEscDetected;

    private final Runnable poll = new Runnable() {
        @Override public void run() {
            if (ble != null && ble.isReady() && deltaEscDetected) {
                ble.sendConfig(SescProtocol.DIAG, new byte[0]);
                handler.postDelayed(this, 500);
            }
        }
    };

    @Override protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        LinearLayout root = new LinearLayout(this);
        root.setOrientation(LinearLayout.VERTICAL);
        int pad = dp(18);
        root.setPadding(pad,pad,pad,pad);

        TextView title = new TextView(this);
        title.setText("DeltaESC Diag 0.4.1");
        title.setTextSize(25);
        title.setTypeface(Typeface.DEFAULT, Typeface.BOLD);
        root.addView(title, matchWrap());

        TextView note = new TextView(this);
        note.setText("READ-ONLY · NinebotCrypto · Stock G30 Dashboard\nKeine Motor-/ARM-Befehle in dieser App.");
        note.setTextSize(14);
        note.setPadding(0,dp(5),0,dp(14));
        root.addView(note, matchWrap());

        connect = new Button(this);
        connect.setText("Mit G30 verbinden");
        connect.setOnClickListener(v -> connect());
        root.addView(connect, matchWrap());

        status = new TextView(this);
        status.setText("Nicht verbunden");
        status.setTextSize(17);
        status.setPadding(0,dp(14),0,dp(8));
        root.addView(status, matchWrap());

        identity = new TextView(this);
        identity.setTypeface(Typeface.MONOSPACE);
        identity.setText("DeltaESC: --");
        identity.setTextSize(15);
        root.addView(identity, matchWrap());

        data = new TextView(this);
        data.setTypeface(Typeface.MONOSPACE);
        data.setTextSize(15);
        data.setPadding(0,dp(12),0,0);
        data.setText("Warte auf Diagnosedaten…");
        root.addView(data, matchWrap());

        ScrollView scroll = new ScrollView(this);
        scroll.addView(root);
        setContentView(scroll);

        ble = new NinebotBleClient(this,this);
    }

    private LinearLayout.LayoutParams matchWrap() {
        return new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.WRAP_CONTENT);
    }

    private int dp(int v) {
        return Math.round(v * getResources().getDisplayMetrics().density);
    }

    private boolean havePermissions() {
        if (Build.VERSION.SDK_INT >= 31) {
            return checkSelfPermission(Manifest.permission.BLUETOOTH_SCAN) == PackageManager.PERMISSION_GRANTED &&
                    checkSelfPermission(Manifest.permission.BLUETOOTH_CONNECT) == PackageManager.PERMISSION_GRANTED;
        }
        return checkSelfPermission(Manifest.permission.ACCESS_FINE_LOCATION) == PackageManager.PERMISSION_GRANTED;
    }

    private void requestPermissionsIfNeeded() {
        if (Build.VERSION.SDK_INT >= 31) {
            requestPermissions(new String[] {
                    Manifest.permission.BLUETOOTH_SCAN,
                    Manifest.permission.BLUETOOTH_CONNECT
            }, REQ_BLE);
        } else {
            requestPermissions(new String[] {
                    Manifest.permission.ACCESS_FINE_LOCATION
            }, REQ_BLE);
        }
    }

    private void connect() {
        handler.removeCallbacks(poll);
        deltaEscDetected = false;
        identity.setText("DeltaESC: --");
        data.setText("Warte auf Diagnosedaten…");
        if (!havePermissions()) {
            requestPermissionsIfNeeded();
            return;
        }
        ble.scanAndConnect();
    }

    @Override public void onRequestPermissionsResult(int requestCode, String[] permissions, int[] grantResults) {
        super.onRequestPermissionsResult(requestCode,permissions,grantResults);
        if (requestCode != REQ_BLE) return;
        if (havePermissions()) ble.scanAndConnect();
        else status.setText("Bluetooth-Berechtigung fehlt");
    }

    @Override public void onStatus(String text) {
        runOnUiThread(() -> status.setText(text));
    }

    @Override public void onReady(boolean encrypted) {
        runOnUiThread(() -> {
            status.setText("NinebotCrypto authentifiziert · prüfe DeltaESC…");
            connect.setText("Neu verbinden");
            ble.sendConfig(SescProtocol.HELLO,new byte[0]);
            handler.postDelayed(() -> {
                if (!deltaEscDetected && ble != null && ble.isReady()) {
                    status.setText("Crypto OK, aber kein DeltaESC HELLO");
                    ble.sendConfig(SescProtocol.HELLO,new byte[0]);
                }
            },1000);
        });
    }

    @Override public void onFrame(SescProtocol.Frame frame) {
        runOnUiThread(() -> {
            if (frame == null || frame.src != SescProtocol.ESC_ADDR ||
                    frame.dst != SescProtocol.APP_ADDR || frame.cmd != SescProtocol.CMD_CONFIG) return;

            if (frame.arg == SescProtocol.HELLO) {
                SescProtocol.Hello h = SescProtocol.parseHello(frame.payload);
                if (h == null) return;
                deltaEscDetected = true;
                identity.setText(String.format(Locale.US,
                        "DeltaESC %d.%d.%d  proto=%d  flags=0x%02X\n" +
                        "sensorless=%s  BLEdiag=%s  active-capable=%s",
                        h.major,h.minor,h.patch,h.protocol,h.flags,
                        yes(h.sensorless()),yes(h.bleDiag()),yes(h.activeCapable())));
                status.setText("DeltaESC über Stock-BLE erreichbar");
                handler.removeCallbacks(poll);
                handler.post(poll);
                return;
            }

            if (frame.arg == SescProtocol.DIAG) {
                SescProtocol.Diag d = SescProtocol.parseDiag(frame.payload);
                if (d != null) renderDiag(d);
            }
        });
    }

    private void renderDiag(SescProtocol.Diag d) {
        double ctrlLastUs = d.controlLastCycles / 64.0;
        double ctrlMaxUs = d.controlMaxCycles / 64.0;
        double isrLastUs = d.isrLastCycles / 64.0;
        double isrMaxUs = d.isrMaxCycles / 64.0;

        data.setText(String.format(Locale.US,
                "STATE\n" +
                " armed          %s\n" +
                " safety         0x%08X\n" +
                " OC trips       %d\n" +
                " hard overruns  %d\n\n" +
                "ADC / PWM SYNC\n" +
                " samples        %d\n" +
                " interval min   %d cyc\n" +
                " interval max   %d cyc\n" +
                " expected       4000 cyc\n\n" +
                "CURRENT ADC RAW\n" +
                " IA             %d\n" +
                " IB             %d\n" +
                " IC             %d\n" +
                " VBUS raw       %d\n" +
                " residual       %d counts\n" +
                " peak           %d counts\n\n" +
                "TIMING @ 64 MHz\n" +
                " control        %d / %d cyc\n" +
                "                %.1f / %.1f us\n" +
                " ADC ISR        %d / %d cyc\n" +
                "                %.1f / %.1f us\n\n" +
                "PASS target: safety=0, OC=0, hard=0,\n" +
                "ADC interval close to 4000 cycles.",
                yes(d.armed()),d.safetyLatch,d.overCurrentTrips,d.hardOverruns,
                d.adcSamples,d.sampleMinCycles,d.sampleMaxCycles,
                d.adcIa,d.adcIb,d.adcIc,d.adcVbus,d.currentResidual,d.currentPeak,
                d.controlLastCycles,d.controlMaxCycles,ctrlLastUs,ctrlMaxUs,
                d.isrLastCycles,d.isrMaxCycles,isrLastUs,isrMaxUs));
    }

    private static String yes(boolean b) {
        return b ? "YES" : "no";
    }

    @Override public void onDisconnected() {
        runOnUiThread(() -> {
            handler.removeCallbacks(poll);
            deltaEscDetected = false;
            status.setText("Verbindung getrennt");
        });
    }

    @Override protected void onDestroy() {
        handler.removeCallbacksAndMessages(null);
        if (ble != null) ble.disconnect();
        super.onDestroy();
    }
}
