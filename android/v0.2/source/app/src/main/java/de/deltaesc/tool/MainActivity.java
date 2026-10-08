package de.deltaesc.tool;

import android.app.Activity;
import android.bluetooth.BluetoothDevice;
import android.content.Intent;
import android.graphics.Color;
import android.os.*;
import android.provider.Settings;
import android.view.*;
import android.widget.*;
import java.text.SimpleDateFormat;
import java.util.*;

public class MainActivity extends Activity implements BleUartClient.Listener {
    private static final int REQ_PERMS=100;
    private final Handler handler=new Handler(Looper.getMainLooper());
    private final SimpleDateFormat timeFmt=new SimpleDateFormat("HH:mm:ss.SSS",Locale.GERMANY);
    private BleUartClient ble; private LinearLayout devicesBox; private TextView connectionText,statusText,logText;

    @Override protected void onCreate(Bundle b){super.onCreate(b);ble=new BleUartClient(this,this);setContentView(buildUi());ensurePermissions();}
    @Override protected void onDestroy(){handler.removeCallbacksAndMessages(null);if(ble!=null)ble.disconnect();super.onDestroy();}

    private View buildUi(){
        ScrollView sc=new ScrollView(this);LinearLayout root=new LinearLayout(this);root.setOrientation(LinearLayout.VERTICAL);root.setPadding(dp(14),dp(14),dp(14),dp(30));root.setBackgroundColor(Color.rgb(16,18,22));sc.addView(root);
        root.addView(text("DeltaESC Preflight",28,true));
        TextView sub=text("G30D • NinebotCrypto/MIC • v0.2.1 • READ-ONLY",13,true);sub.setTextColor(Color.rgb(255,130,120));root.addView(sub);
        root.addView(section("VERBINDUNG"));connectionText=text("Nicht verbunden",16,true);root.addView(connectionText);
        LinearLayout row=row();row.addView(button("BLE scannen",v->startScan()));row.addView(button("Trennen",v->ble.disconnect()));root.addView(row);
        devicesBox=new LinearLayout(this);devicesBox.setOrientation(LinearLayout.VERTICAL);root.addView(devicesBox);
        root.addView(section("SICHERER LESETEST"));
        TextView note=text("Diese APK sendet keine ESC-Schreibbefehle. Erwartung auf Stock: FW 0x0420, D0 ohne DESC/0x0603. Nach DeltaESC SAFE: D0 muss exakt DESC + 0x0603 liefern.",13,false);note.setTextColor(Color.LTGRAY);root.addView(note);
        LinearLayout rr=row();rr.addView(button("Stock FW 0x1A",v->sendRead(0x1A,2,"READ stock FW")));rr.addView(button("Delta-ID D0",v->sendRead(0xD0,16,"READ Delta-ID")));root.addView(rr);
        statusText=mono("Noch keine Daten.");root.addView(statusText);
        root.addView(section("BUS LOG"));logText=mono("");logText.setTextIsSelectable(true);root.addView(logText);
        return sc;
    }
    private void ensurePermissions(){if(!ble.bluetoothAvailable()){connectionText.setText("Bluetooth LE nicht verfügbar");return;}if(!ble.permissionsGranted())requestPermissions(BleUartClient.runtimePermissions(),REQ_PERMS);if(!ble.bluetoothEnabled()){connectionText.setText("Bluetooth ist aus");startActivity(new Intent(Settings.ACTION_BLUETOOTH_SETTINGS));}}
    private void startScan(){ensurePermissions();if(!ble.permissionsGranted()||!ble.bluetoothEnabled())return;devicesBox.removeAllViews();ble.startScan();connectionText.setText("Suche NBScooter/MIScooter/Ninebot/NUS …");}
    private void sendRead(int reg,int n,String tag){if(!ble.isReady()){toast("NinebotCrypto noch nicht angemeldet");return;}byte[] p=NinebotProtocol.read(reg,n);if(ble.write(p))appendLog("TX "+tag+"  "+Hex.of(p));}

    @Override public void onReady(boolean ready){runOnUiThread(()->{if(ready){appendLog("NinebotCrypto/MIC bereit. Automatischer READ-ONLY Preflight.");handler.postDelayed(()->sendRead(0x1A,2,"READ stock FW"),250);handler.postDelayed(()->sendRead(0xD0,16,"READ Delta-ID"),650);}});}
    @Override public void onBytes(byte[] data){runOnUiThread(()->{appendLog("RX "+Hex.of(data));try{NinebotProtocol.Frame f=NinebotProtocol.decode(data);if(f.command==0x1A&&f.payload.length>=2){int v=DeltaEscProtocol.u16(f.payload,0);statusText.setText(String.format(Locale.US,"ESC FW raw: 0x%04X\n",v)+statusText.getText());}else if(f.command==0xD0){if(DeltaEscProtocol.isDeltaEscIdentity(f)){statusText.setText("DELTAESC QUALIFIZIERT: DESC + build 0x0603\n"+DeltaEscProtocol.describe(f));connectionText.setText("DeltaESC v0.6.3 eindeutig erkannt");}else{statusText.setText("STOCK/ANDERE FW: D0 ist NICHT DESC/0x0603\nD0: "+Hex.of(f.payload));connectionText.setText("Stock/andere Firmware • Writes bleiben hardwareseitig gesperrt");}}}catch(Exception e){appendLog("Decode Fehler: "+e.getMessage());}});}
    @Override public void onLog(String s){runOnUiThread(()->appendLog("BLE "+s));}
    @Override public void onConnection(boolean c,String label){runOnUiThread(()->connectionText.setText(label));}
    @Override public void onDevice(BluetoothDevice d,int rssi){runOnUiThread(()->{String n;try{n=d.getName();}catch(SecurityException e){n=null;}if(n==null)n="Unbenannt";Button b=button(n+"   "+rssi+" dBm\n"+d.getAddress(),v->ble.connect(d));devicesBox.addView(b,full());});}

    private void appendLog(String s){if(logText==null)return;String old=logText.getText().toString();if(old.length()>20000)old=old.substring(old.length()-15000);logText.setText(old+timeFmt.format(new Date())+"  "+s+"\n");}
    private TextView section(String s){TextView v=text(s,13,true);v.setTextColor(Color.rgb(255,90,80));v.setPadding(0,dp(20),0,dp(6));return v;}
    private TextView text(String s,int sp,boolean bold){TextView v=new TextView(this);v.setText(s);v.setTextSize(sp);v.setTextColor(Color.WHITE);if(bold)v.setTypeface(android.graphics.Typeface.DEFAULT,android.graphics.Typeface.BOLD);return v;}
    private TextView mono(String s){TextView v=text(s,13,false);v.setTypeface(android.graphics.Typeface.MONOSPACE);v.setPadding(dp(8),dp(8),dp(8),dp(8));v.setBackgroundColor(Color.rgb(26,29,34));return v;}
    private Button button(String label,View.OnClickListener l){Button b=new Button(this);b.setText(label);b.setAllCaps(false);b.setOnClickListener(l);LinearLayout.LayoutParams p=new LinearLayout.LayoutParams(0,LinearLayout.LayoutParams.WRAP_CONTENT,1f);p.setMargins(dp(2),dp(3),dp(2),dp(3));b.setLayoutParams(p);return b;}
    private LinearLayout row(){LinearLayout l=new LinearLayout(this);l.setOrientation(LinearLayout.HORIZONTAL);l.setGravity(Gravity.CENTER_VERTICAL);return l;}
    private LinearLayout.LayoutParams full(){return new LinearLayout.LayoutParams(LinearLayout.LayoutParams.MATCH_PARENT,LinearLayout.LayoutParams.WRAP_CONTENT);}
    private int dp(int x){return Math.round(x*getResources().getDisplayMetrics().density);} private void toast(String s){Toast.makeText(this,s,Toast.LENGTH_SHORT).show();}
    @Override public void onRequestPermissionsResult(int r,String[] p,int[] g){super.onRequestPermissionsResult(r,p,g);if(r==REQ_PERMS&&ble.permissionsGranted())connectionText.setText("Bereit zum Scannen");}
}
