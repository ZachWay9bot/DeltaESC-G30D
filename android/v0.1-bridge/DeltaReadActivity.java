package de.ebics.g30bench;

import android.Manifest;
import android.app.Activity;
import android.bluetooth.BluetoothDevice;
import android.content.ClipData;
import android.content.ClipboardManager;
import android.content.Context;
import android.content.pm.PackageManager;
import android.content.pm.ApplicationInfo;
import dalvik.system.DexClassLoader;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.os.SystemClock;
import android.view.View;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;
import java.lang.reflect.*;
import java.util.*;

public final class DeltaReadActivity extends Activity {
  private static final String PK="de.ebics.g30bench.";
  private final Handler main=new Handler(Looper.getMainLooper());
  private Class<?> bc,sc,pc,replyType,failType;
  private ClassLoader originalLoader;
  private Object ble,session;
  private LinearLayout devices,layout;
  private TextView status,log;
  private Button readAll;
  private final StringBuilder trace=new StringBuilder();
  private final LinkedHashMap<Integer,String> registers=new LinkedHashMap<>();
  private boolean linked,busy,identified;
  private String last="Bereit";
  private int build=0;
  private int nextReg=0;

  @Override public void onCreate(Bundle b) {super.onCreate(b);makeUi();try{initOriginal();}catch(Throwable e){line("Originalklassen fehlen: "+error(e));}permissions();}
  @Override public void onDestroy(){disconnect();main.removeCallbacksAndMessages(null);super.onDestroy();}
  static Object invoke(Class<?> c,Object o,String name,Class<?>[] sig,Object...args)throws Exception{
    Method m=c.getDeclaredMethod(name,sig);m.setAccessible(true);return m.invoke(o,args);
  }
  static Object field(Object o,String n)throws Exception{Field f=o.getClass().getDeclaredField(n);f.setAccessible(true);return f.get(o);}
  static String error(Throwable e){while(e.getCause()!=null)e=e.getCause();return e.getClass().getSimpleName()+": "+e.getMessage();}
  static String hex(byte[] b){StringBuilder t=new StringBuilder();for(byte v:b)t.append(String.format(java.util.Locale.US,"%02X",v&255));return t.toString();}
  static Object objectMethod(Object p,Method m,Object[] a){
    if(m.getName().equals("toString"))return "OriginalBLECallback";
    if(m.getName().equals("hashCode"))return System.identityHashCode(p);
    if(m.getName().equals("equals"))return p==a[0];
    return null;
  }
  void initOriginal()throws Exception {
    ClassLoader loader=getClassLoader();
    bc=Class.forName(PK+"BleClient",true,loader);
    sc=Class.forName(PK+"Session",true,loader);
    pc=Class.forName(PK+"Packet",true,loader);
    Class<?> io=Class.forName(PK+"Session$IO",true,loader);
    Class<?> listener=Class.forName(PK+"BleClient$Listener",true,loader);
    replyType=Class.forName(PK+"Session$Reply",true,loader);
    failType=Class.forName(PK+"Session$Failure",true,loader);
    Object ioProxy=Proxy.newProxyInstance(loader,new Class[]{io},(p,m,a)->{
      switch(m.getName()){
        case "now":return SystemClock.elapsedRealtime();
        case "later":main.postDelayed((Runnable)a[0],((Number)a[1]).longValue());return null;
        case "changed":refresh();return null;
        case "send":
          /* Identical to original MainActivity$1.send: BleClient.send(packet, Session.isKnown()). */
          boolean known=(Boolean)invoke(sc,session,"isKnown",new Class[0]);
          invoke(bc,ble,"send",new Class[]{pc,boolean.class},a[0],known);return null;
        default:return objectMethod(p,m,a);
      }
    });
    session=sc.getConstructor(io).newInstance(ioProxy);
    Object cb=Proxy.newProxyInstance(loader,new Class[]{listener},(p,m,a)->{
      switch(m.getName()){
        case "device":main.post(()->addDevice((BluetoothDevice)a[0],(String)a[1],((Number)a[2]).intValue()));return null;
        case "status":line("BLE: "+a[0]);return null;
        case "metadata":return null;
        case "packet":invoke(sc,session,"receive",new Class[]{pc},a[0]);return null;
        case "ready":
          linked=true;
          invoke(sc,session,"connected",new Class[]{String.class},a[0]);
          line("Original-NinebotCrypto Handshake abgeschlossen: "+a[0]);refresh();return null;
        case "disconnected":
          linked=false;busy=false;identified=false;registers.clear();
          invoke(sc,session,"disconnect",new Class[0]);line("Getrennt");refresh();return null;
        default:return objectMethod(p,m,a);
      }
    });
    ble=bc.getConstructor(Context.class,listener).newInstance(this,cb);
    line("Installierte G30 Bench BLE 0.1.0 geladen; BleClient/Session laufen als Original-Bytecode.");
  }
  boolean permissions(){
    ArrayList<String> need=new ArrayList<>();
    if(Build.VERSION.SDK_INT>=31){
      if(checkSelfPermission(Manifest.permission.BLUETOOTH_SCAN)!=PackageManager.PERMISSION_GRANTED)need.add(Manifest.permission.BLUETOOTH_SCAN);
      if(checkSelfPermission(Manifest.permission.BLUETOOTH_CONNECT)!=PackageManager.PERMISSION_GRANTED)need.add(Manifest.permission.BLUETOOTH_CONNECT);
    }else if(Build.VERSION.SDK_INT>=23 && checkSelfPermission(Manifest.permission.ACCESS_FINE_LOCATION)!=PackageManager.PERMISSION_GRANTED)need.add(Manifest.permission.ACCESS_FINE_LOCATION);
    if(!need.isEmpty()){requestPermissions(need.toArray(new String[0]),100);return false;}return true;
  }
  void scan(){if(ble==null||!permissions())return;devices.removeAllViews();try{
      invoke(bc,ble,"scan",new Class[0]);line("Scan gestartet (Original 0.1.0)");
    }catch(Exception e){line("Scan-Fehler "+error(e));}}
  void addDevice(BluetoothDevice d,String name,int rssi){
    String n=name==null?"Unbenannt":name;
    Button button=button(n+"  "+rssi+" dBm");devices.addView(button);
    button.setOnClickListener(v->{try{
      disconnect();invoke(bc,ble,"connect",new Class[]{BluetoothDevice.class,String.class},d,n);
      line("Verbinde über Original 0.1.0: "+n);
    }catch(Exception e){line("Verbindungsfehler "+error(e));}});
  }
  void disconnect(){linked=false;busy=false;identified=false;registers.clear();if(bc!=null&&ble!=null)try{invoke(bc,ble,"disconnect",new Class[0]);}catch(Exception ignored){}refresh();}
  void readD0(){if(!linked||busy){line("Nicht angemeldet oder bereits beschäftigt");return;}identified=false;registers.clear();read(0xD0);}
  void readAll(){if(!linked||busy||!identified){line("D1-D9 gesperrt, erst D0 prüfen.");return;}nextReg=0xD1;read(nextReg);}
  void read(int reg){
    if(!linked||busy)return;if(reg!=0xD0&&!identified)return;
    busy=true;refresh();
    try{
      Object reply=Proxy.newProxyInstance(originalLoader,new Class[]{replyType},(p,m,a)->{
        if(m.getName().equals("accept")){
          Object pkt=a[0];int src=(Integer)field(pkt,"src"),cmd=(Integer)field(pkt,"cmd"),arg=(Integer)field(pkt,"arg");
          byte[] data=(byte[])field(pkt,"data");
          if(src!=0x20||cmd!=0x04||arg!=reg){line("Unerwarteter ESC-Frame, verworfen");busy=false;refresh();return null;}
          registers.put(reg,hex(data));
          if(reg==0xD0){
            identified=data.length==16&&data[0]=='D'&&data[1]=='E'&&data[2]=='S'&&data[3]=='C';
            if(identified){build=(data[6]&255)|((data[7]&255)<<8);identified=build==0x0606;}
            line(identified?String.format(java.util.Locale.US,"DeltaESC-Signatur bestätigt (Build %04X)",build):
                "Stock oder unbekannte Firmware: D1-D9 gesperrt.");
          }else line(String.format(java.util.Locale.US,"READ %02X: %s",reg,hex(data)));
          busy=false;refresh();
          if(identified&&nextReg==reg&&reg<0xD9){nextReg=reg+1;main.postDelayed(()->read(nextReg),180);}
        }return objectMethod(p,m,a);
      });
      Object failure=Proxy.newProxyInstance(originalLoader,new Class[]{failType},(p,m,a)->{
        if(m.getName().equals("run")){busy=false;line(String.format(java.util.Locale.US,"Timeout bei %02X",reg));refresh();}
        return objectMethod(p,m,a);
      });
      int length=reg==0xD0?16:reg==0xD1?14:reg==0xD2?5:reg==0xD3||reg==0xD4||reg==0xD5?4:reg==0xD6?14:reg==0xD7?8:reg==0xD8?12:6;
      invoke(sc,session,"readRegister",new Class[]{int.class,int.class,int.class,replyType,failType},0x20,reg,length,reply,failure);
      line(String.format(java.util.Locale.US,"TX original Session.readRegister(20,%02X,%d)",reg,length));
    }catch(Throwable e){busy=false;line("Read-Fehler "+error(e));refresh();}
  }
  void copy(){
    StringBuilder s=new StringBuilder("G30 Bench original BLE v0.1.0 / DeltaESC read-only\n");
    s.append("linked=").append(linked).append(" signature=").append(identified).append('\n');
    for(Map.Entry<Integer,String> e:registers.entrySet())s.append(String.format(java.util.Locale.US,"%02X=%s\n",e.getKey(),e.getValue()));
    ((ClipboardManager)getSystemService(Context.CLIPBOARD_SERVICE)).setPrimaryClip(ClipData.newPlainText("DeltaESC",s.toString()));
    line("Diagnose kopiert");
  }
  void line(String t){main.post(()->{last=t;if(trace.length()>12000)trace.delete(0,5000);trace.append(t).append('\n');if(log!=null)log.setText(trace.toString());refresh();});}
  void refresh(){main.post(()->{
    if(status!=null)status.setText("G30 v0.1.0: "+(linked?"angemeldet":"nicht verbunden")+"\n"+(identified?"DeltaESC: DESC / "+String.format("%04X",build):"DeltaESC: nicht bestätigt")+"\n"+last);
    if(readAll!=null)readAll.setEnabled(linked&&!busy&&identified);
  });}
  Button button(String txt){Button b=new Button(this);b.setText(txt);b.setAllCaps(false);return b;}
  void makeUi(){
    ScrollView scroll=new ScrollView(this);LinearLayout col=new LinearLayout(this);col.setOrientation(LinearLayout.VERTICAL);col.setPadding(20,20,20,40);col.setBackgroundColor(0xff111a27);scroll.addView(col);
    TextView h=new TextView(this);h.setText("DeltaESC 0.6.6  •  G30 Original-Transport");h.setTextColor(-1);h.setTextSize(22);col.addView(h);
    status=new TextView(this);status.setTextColor(-1);status.setTextSize(16);col.addView(status);
    Button scan=button("Scooter suchen");scan.setOnClickListener(v->scan());col.addView(scan);
    Button dis=button("Trennen");dis.setOnClickListener(v->disconnect());col.addView(dis);
    devices=new LinearLayout(this);devices.setOrientation(LinearLayout.VERTICAL);col.addView(devices);
    Button id=button("DeltaESC D0 identifizieren");id.setOnClickListener(v->readD0());col.addView(id);
    readAll=button("D1–D9 nur bei bestätigtem DESC/0606");readAll.setEnabled(false);readAll.setOnClickListener(v->readAll());col.addView(readAll);
    Button cp=button("Bericht kopieren");cp.setOnClickListener(v->copy());col.addView(cp);
    log=new TextView(this);log.setTextColor(-1);log.setTextSize(12);log.setTypeface(android.graphics.Typeface.MONOSPACE);col.addView(log);
    TextView footer=new TextView(this);footer.setText("READ-ONLY Companion. Original G30 Bench BLE 0.1.0 muss installiert bleiben. Kein eigener BLE/Crypto-Code.");footer.setTextColor(0xffff9d7a);col.addView(footer);
    setContentView(scroll);refresh();
  }
}