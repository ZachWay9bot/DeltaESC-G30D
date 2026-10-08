package de.deltaesc.tool;

import android.Manifest;
import android.annotation.SuppressLint;
import android.app.Activity;
import android.bluetooth.*;
import android.bluetooth.le.*;
import android.content.Context;
import android.content.pm.PackageManager;
import android.os.*;
import android.os.ParcelUuid;
import java.io.ByteArrayOutputStream;
import java.util.*;

final class BleUartClient {
    interface Listener {
        void onLog(String s); void onDevice(BluetoothDevice device,int rssi);
        void onConnection(boolean connected,String label); void onReady(boolean ready);
        void onBytes(byte[] data);
    }
    static final UUID NUS_SERVICE=UUID.fromString("6e400001-b5a3-f393-e0a9-e50e24dcca9e");
    static final UUID NUS_RX=UUID.fromString("6e400002-b5a3-f393-e0a9-e50e24dcca9e");
    static final UUID NUS_TX=UUID.fromString("6e400003-b5a3-f393-e0a9-e50e24dcca9e");
    static final UUID CCCD=UUID.fromString("00002902-0000-1000-8000-00805f9b34fb");

    private final Activity activity; private final Listener listener; private final Handler main=new Handler(Looper.getMainLooper());
    private final BluetoothAdapter adapter; private BluetoothLeScanner scanner; private BluetoothGatt gatt;
    private BluetoothGattCharacteristic rx,tx; private boolean scanning,nusReady,ready,txBusy; private int discoveryAttempts;
    private final Map<String,BluetoothDevice> seen=new LinkedHashMap<>(); private final ArrayDeque<byte[]> txQueue=new ArrayDeque<>();
    private NinebotCrypto crypto; private final EncStreamParser encParser=new EncStreamParser();
    private byte[] pairSerial; private int hsStage,hsTries,badMic; private String cryptoName="";

    BleUartClient(Activity a,Listener l){activity=a;listener=l;BluetoothManager bm=(BluetoothManager)a.getSystemService(Context.BLUETOOTH_SERVICE);adapter=bm!=null?bm.getAdapter():null;}
    boolean bluetoothAvailable(){return adapter!=null;} boolean bluetoothEnabled(){return adapter!=null&&adapter.isEnabled();} boolean isReady(){return ready;}
    static String[] runtimePermissions(){if(Build.VERSION.SDK_INT>=31)return new String[]{Manifest.permission.BLUETOOTH_SCAN,Manifest.permission.BLUETOOTH_CONNECT};return new String[]{Manifest.permission.ACCESS_FINE_LOCATION};}
    boolean permissionsGranted(){for(String p:runtimePermissions())if(activity.checkSelfPermission(p)!=PackageManager.PERMISSION_GRANTED)return false;return true;}

    @SuppressLint("MissingPermission") void startScan(){if(!permissionsGranted()||adapter==null||!adapter.isEnabled())return;stopScan();seen.clear();scanner=adapter.getBluetoothLeScanner();if(scanner==null)return;scanning=true;listener.onLog("BLE Scan gestartet");scanner.startScan(scanCallback);main.postDelayed(this::stopScan,12000);}
    @SuppressLint("MissingPermission") void stopScan(){if(scanning&&scanner!=null&&permissionsGranted())try{scanner.stopScan(scanCallback);}catch(Exception ignored){}scanning=false;}
    @SuppressLint("MissingPermission") void connect(BluetoothDevice d){
        stopScan();disconnect();String n=null;try{n=d.getName();}catch(SecurityException ignored){}
        cryptoName=(n==null||n.isEmpty())?"NBScooter2020":n;crypto=new NinebotCrypto(cryptoName);encParser.reset();pairSerial=null;badMic=0;hsStage=0;hsTries=0;discoveryAttempts=0;
        listener.onConnection(false,"Verbinde "+safeName(d));gatt=d.connectGatt(activity,false,callback,BluetoothDevice.TRANSPORT_LE);
    }
    @SuppressLint("MissingPermission") void disconnect(){
        main.removeCallbacks(handshakeRetry);main.removeCallbacks(serviceDiscoveryTimeout);ready=false;nusReady=false;txBusy=false;hsStage=0;discoveryAttempts=0;txQueue.clear();listener.onReady(false);
        if(gatt!=null){try{gatt.disconnect();}catch(Exception ignored){}try{gatt.close();}catch(Exception ignored){}}gatt=null;rx=null;tx=null;
    }

    synchronized boolean write(byte[] plain){
        if(!ready||crypto==null||!NinebotProtocol.valid(plain))return false;
        if(NinebotProtocol.isEscWrite(plain)){listener.onLog("BLOCK WRITE: v0.2 ist absichtlich READ-ONLY");return false;}
        try{return queueEncrypted(crypto.encrypt(plain));}catch(Exception e){listener.onLog("Crypto TX Fehler: "+e.getMessage());return false;}
    }
    private synchronized boolean queueEncrypted(byte[] data){if(!nusReady||gatt==null||rx==null)return false;txQueue.add(Arrays.copyOf(data,data.length));drainQueue();return true;}
    @SuppressLint("MissingPermission") private synchronized void drainQueue(){
        if(txBusy||txQueue.isEmpty()||gatt==null||rx==null)return;byte[] data=txQueue.poll();txBusy=true;boolean ok;
        if(Build.VERSION.SDK_INT>=33)ok=gatt.writeCharacteristic(rx,data,BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT)==android.bluetooth.BluetoothStatusCodes.SUCCESS;
        else{rx.setWriteType(BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT);rx.setValue(data);ok=gatt.writeCharacteristic(rx);}
        if(!ok){txBusy=false;listener.onLog("BLE write konnte nicht gestartet werden");main.postDelayed(this::drainQueue,80);}
    }
    private void startHandshake(){ready=false;listener.onReady(false);hsStage=1;hsTries=0;listener.onConnection(true,"Melde am Dashboard an (NinebotCrypto)");sendHandshake(NinebotProtocol.init());main.removeCallbacks(handshakeRetry);main.postDelayed(handshakeRetry,3000);}
    private void sendHandshake(byte[] plain){if(crypto==null)return;try{queueEncrypted(crypto.encrypt(plain));listener.onLog("Handshake TX cmd=0x"+String.format("%02X",plain[5]&255));}catch(Exception e){listener.onLog("Handshake TX Fehler: "+e.getMessage());}}
    private final Runnable handshakeRetry=new Runnable(){@Override public void run(){
        if(ready||!nusReady)return;hsTries++;if(hsTries>35){listener.onConnection(true,"NinebotCrypto Anmeldung fehlgeschlagen; neu verbinden");return;}
        if(hsStage==1){listener.onConnection(true,"Keine 5B-Antwort; neu verbinden empfohlen");return;}
        if(hsStage==2){sendHandshake(NinebotProtocol.setKey());main.postDelayed(this,900);}
        else if(hsStage==3&&pairSerial!=null){sendHandshake(NinebotProtocol.pair(pairSerial));main.postDelayed(this,700);}
    }};
    private void handlePlain(byte[] plain){
        NinebotProtocol.Frame f;try{f=NinebotProtocol.decode(plain);}catch(Exception e){listener.onLog("Plain frame ungültig");return;}
        if(f.address==NinebotProtocol.BLE&&f.dst==NinebotProtocol.PHONE&&f.type==0x5B&&f.payload.length>=30){
            pairSerial=Arrays.copyOfRange(f.payload,16,30);hsStage=2;hsTries=0;listener.onConnection(true,"Power-Taste am Scooter kurz drücken, um die Bluetooth-Anmeldung zu bestätigen");sendHandshake(NinebotProtocol.setKey());main.removeCallbacks(handshakeRetry);main.postDelayed(handshakeRetry,900);return;
        }
        if(f.address==NinebotProtocol.BLE&&f.dst==NinebotProtocol.PHONE&&f.type==0x5C){if(f.command==1&&pairSerial!=null){hsStage=3;hsTries=0;main.removeCallbacks(handshakeRetry);sendHandshake(NinebotProtocol.pair(pairSerial));main.postDelayed(handshakeRetry,700);}return;}
        if(f.address==NinebotProtocol.BLE&&f.dst==NinebotProtocol.PHONE&&f.type==0x5D&&f.command==1){main.removeCallbacks(handshakeRetry);hsStage=4;ready=true;listener.onReady(true);listener.onConnection(true,"NinebotCrypto/MIC bestätigt • "+cryptoName);return;}
        if(ready)listener.onBytes(plain);
    }
    private void handleIncoming(byte[] chunk){for(byte[] enc:encParser.push(chunk)){try{handlePlain(crypto.decryptVerified(enc));}catch(SecurityException e){badMic++;listener.onLog("RX verworfen: "+e.getMessage()+" (#"+badMic+")");}catch(Exception e){listener.onLog("RX Crypto Fehler: "+e.getMessage());}}}
    @SuppressLint("MissingPermission") private String safeName(BluetoothDevice d){try{String n=d.getName();return(n==null||n.isEmpty())?d.getAddress():n+" ("+d.getAddress()+")";}catch(SecurityException e){return"BLE Gerät";}}

    @SuppressLint("MissingPermission") private void startServiceDiscovery(BluetoothGatt g){
        if(g==null)return;
        discoveryAttempts++;
        listener.onLog("GATT Service Discovery Start #"+discoveryAttempts);
        boolean started=false;
        try{started=g.discoverServices();}catch(Exception e){listener.onLog("discoverServices Exception: "+e.getClass().getSimpleName()+": "+e.getMessage());}
        if(!started){
            listener.onLog("discoverServices() abgelehnt");
            if(discoveryAttempts<3)main.postDelayed(()->{if(g==gatt&&!nusReady)startServiceDiscovery(g);},600);
            else listener.onConnection(true,"GATT Service Discovery konnte nicht gestartet werden");
            return;
        }
        main.removeCallbacks(serviceDiscoveryTimeout);
        main.postDelayed(serviceDiscoveryTimeout,8000);
    }
    private final Runnable serviceDiscoveryTimeout=new Runnable(){@Override public void run(){
        if(nusReady||gatt==null)return;
        listener.onLog("GATT Service Discovery Timeout nach 8 s");
        listener.onConnection(true,"BLE verbunden, aber Service Discovery ohne Antwort");
    }};

    private final ScanCallback scanCallback=new ScanCallback(){@Override public void onScanResult(int t,ScanResult r){BluetoothDevice d=r.getDevice();String key=d.getAddress();if(seen.containsKey(key))return;boolean interesting=false;String name=null;try{name=d.getName();}catch(SecurityException ignored){}if(name!=null){String low=name.toLowerCase();interesting=low.contains("miscooter")||low.contains("ninebot")||low.contains("scooter");}if(r.getScanRecord()!=null&&r.getScanRecord().getServiceUuids()!=null)for(ParcelUuid u:r.getScanRecord().getServiceUuids())if(NUS_SERVICE.equals(u.getUuid())){interesting=true;break;}if(interesting){seen.put(key,d);listener.onDevice(d,r.getRssi());}}@Override public void onScanFailed(int e){listener.onLog("BLE Scanfehler: "+e);}};
    private final BluetoothGattCallback callback=new BluetoothGattCallback(){
        @SuppressLint("MissingPermission") @Override public void onConnectionStateChange(BluetoothGatt g,int status,int state){listener.onLog("GATT state="+state+" status="+status);if(status!=BluetoothGatt.GATT_SUCCESS){listener.onConnection(false,"GATT Fehler "+status);try{g.close();}catch(Exception ignored){}if(g==gatt)gatt=null;return;}if(state==BluetoothProfile.STATE_CONNECTED){listener.onConnection(true,"BLE verbunden; suche UART-Service");startServiceDiscovery(g);}else if(state==BluetoothProfile.STATE_DISCONNECTED){ready=false;nusReady=false;main.removeCallbacks(handshakeRetry);main.removeCallbacks(serviceDiscoveryTimeout);listener.onReady(false);listener.onConnection(false,"Getrennt");try{g.close();}catch(Exception ignored){}if(g==gatt)gatt=null;}}
        @SuppressLint("MissingPermission") @Override public void onServicesDiscovered(BluetoothGatt g,int status){main.removeCallbacks(serviceDiscoveryTimeout);listener.onLog("Services discovered status="+status);if(status!=BluetoothGatt.GATT_SUCCESS){listener.onLog("Service discovery fehlgeschlagen: "+status);return;}BluetoothGattService s=g.getService(NUS_SERVICE);if(s==null){listener.onLog("Nordic UART Service nicht gefunden");return;}rx=s.getCharacteristic(NUS_RX);tx=s.getCharacteristic(NUS_TX);if(rx==null||tx==null){listener.onLog("NUS RX/TX fehlt");return;}g.setCharacteristicNotification(tx,true);BluetoothGattDescriptor d=tx.getDescriptor(CCCD);if(d==null){listener.onLog("CCCD fehlt");return;}boolean started;if(Build.VERSION.SDK_INT>=33)started=g.writeDescriptor(d,BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE)==android.bluetooth.BluetoothStatusCodes.SUCCESS;else{d.setValue(BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE);started=g.writeDescriptor(d);}if(!started)listener.onLog("CCCD write konnte nicht gestartet werden");else listener.onLog("CCCD write gestartet");}
        @Override public void onDescriptorWrite(BluetoothGatt g,BluetoothGattDescriptor d,int status){listener.onLog("Descriptor write status="+status);if(CCCD.equals(d.getUuid())&&status==BluetoothGatt.GATT_SUCCESS){nusReady=true;listener.onLog("NUS Notifications aktiv");startHandshake();}}
        @Override public void onCharacteristicWrite(BluetoothGatt g,BluetoothGattCharacteristic c,int status){synchronized(BleUartClient.this){txBusy=false;if(status!=BluetoothGatt.GATT_SUCCESS)listener.onLog("BLE write status="+status);drainQueue();}}
        @Override public void onCharacteristicChanged(BluetoothGatt g,BluetoothGattCharacteristic c){if(NUS_TX.equals(c.getUuid())){byte[] v=c.getValue();if(v!=null)handleIncoming(v.clone());}}
        @Override public void onCharacteristicChanged(BluetoothGatt g,BluetoothGattCharacteristic c,byte[] v){if(NUS_TX.equals(c.getUuid())&&v!=null)handleIncoming(v.clone());}
    };
    private static final class EncStreamParser{
        private final ByteArrayOutputStream b=new ByteArrayOutputStream();void reset(){b.reset();}
        synchronized List<byte[]> push(byte[] c){try{b.write(c);}catch(Exception ignored){}byte[] d=b.toByteArray();List<byte[]> out=new ArrayList<>();int p=0;
            while(p+3<=d.length){if((d[p]&255)!=0x5A||(d[p+1]&255)!=0xA5){p++;continue;}int total=(d[p+2]&255)+13;if(total<13||total>280){p++;continue;}if(p+total>d.length)break;out.add(Arrays.copyOfRange(d,p,p+total));p+=total;}
            b.reset();if(p<d.length)b.write(d,p,d.length-p);return out;}
    }
}
