package net.superfede.capacitor.bleperipheral;

import android.Manifest;
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothGatt;
import android.bluetooth.BluetoothGattCharacteristic;
import android.bluetooth.BluetoothGattDescriptor;
import android.bluetooth.BluetoothGattServer;
import android.bluetooth.BluetoothGattServerCallback;
import android.bluetooth.BluetoothGattService;
import android.bluetooth.BluetoothManager;
import android.bluetooth.le.AdvertiseCallback;
import android.bluetooth.le.AdvertiseData;
import android.bluetooth.le.AdvertiseSettings;
import android.bluetooth.le.BluetoothLeAdvertiser;
import android.content.Context;
import android.content.pm.PackageManager;
import android.os.Build;
import android.os.ParcelUuid;
import android.util.Base64;
import android.util.Log;

import androidx.core.app.ActivityCompat;

import com.getcapacitor.JSArray;
import com.getcapacitor.JSObject;
import com.getcapacitor.Plugin;
import com.getcapacitor.PluginCall;
import com.getcapacitor.PluginMethod;
import com.getcapacitor.annotation.CapacitorPlugin;
import com.getcapacitor.annotation.Permission;

import java.util.HashMap;
import java.util.HashSet;
import java.util.Map;
import java.util.Set;
import java.util.UUID;

@CapacitorPlugin(
    name = "BlePeripheral",
    permissions = {
        @Permission(strings = { Manifest.permission.BLUETOOTH }, alias = "bluetooth"),
        @Permission(strings = { Manifest.permission.BLUETOOTH_ADMIN }, alias = "bluetoothAdmin"),
        @Permission(strings = { Manifest.permission.BLUETOOTH_ADVERTISE }, alias = "bluetoothAdvertise"),
        @Permission(strings = { Manifest.permission.BLUETOOTH_CONNECT }, alias = "bluetoothConnect")
    }
)
public class BlePeripheralPlugin extends Plugin {
    private static final String TAG = "BlePeripheral";
    private static final UUID CLIENT_CHARACTERISTIC_CONFIG_UUID = 
        UUID.fromString("00002902-0000-1000-8000-00805f9b34fb");

    private BluetoothManager bluetoothManager;
    private BluetoothAdapter bluetoothAdapter;
    private BluetoothLeAdvertiser bluetoothLeAdvertiser;
    private BluetoothGattServer gattServer;
    
    private Map<String, BluetoothGattService> services = new HashMap<>();
    private Map<String, BluetoothGattCharacteristic> characteristics = new HashMap<>();
    private Set<BluetoothDevice> connectedDevices = new HashSet<>();
    private Map<String, Set<BluetoothDevice>> notificationSubscriptions = new HashMap<>();

    @PluginMethod
    public void initialize(PluginCall call) {
        try {
            bluetoothManager = (BluetoothManager) getContext().getSystemService(Context.BLUETOOTH_SERVICE);
            if (bluetoothManager == null) {
                call.reject("Bluetooth not supported");
                return;
            }

            bluetoothAdapter = bluetoothManager.getAdapter();
            if (bluetoothAdapter == null) {
                call.reject("Bluetooth adapter not available");
                return;
            }

            if (!bluetoothAdapter.isEnabled()) {
                call.reject("Bluetooth is disabled");
                return;
            }

            bluetoothLeAdvertiser = bluetoothAdapter.getBluetoothLeAdvertiser();
            if (bluetoothLeAdvertiser == null) {
                call.reject("BLE advertising not supported");
                return;
            }

            Log.d(TAG, "BLE Peripheral initialized");
            call.resolve();
        } catch (Exception e) {
            Log.e(TAG, "Initialize failed", e);
            call.reject("Initialize failed: " + e.getMessage());
        }
    }

    @PluginMethod
    public void startAdvertising(PluginCall call) {
        if (!checkPermissions(call)) {
            return;
        }

        try {
            String name = call.getString("name");
            if (name == null || name.isEmpty()) {
                call.reject("Device name is required");
                return;
            }

            // Set device name
            if (ActivityCompat.checkSelfPermission(getContext(), 
                    Manifest.permission.BLUETOOTH_CONNECT) == PackageManager.PERMISSION_GRANTED) {
                bluetoothAdapter.setName(name);
            }

            // Build advertise settings
            AdvertiseSettings.Builder settingsBuilder = new AdvertiseSettings.Builder()
                .setAdvertiseMode(AdvertiseSettings.ADVERTISE_MODE_BALANCED)
                .setConnectable(call.getBoolean("connectable", true))
                .setTimeout(call.getInt("timeout", 0))
                .setTxPowerLevel(AdvertiseSettings.ADVERTISE_TX_POWER_MEDIUM);

            // Build advertise data
            AdvertiseData.Builder dataBuilder = new AdvertiseData.Builder()
                .setIncludeDeviceName(true)
                .setIncludeTxPowerLevel(false);

            // Add service UUIDs
            if (call.hasOption("serviceUuids")) {
                JSArray serviceUuids = call.getArray("serviceUuids");
                for (int i = 0; i < serviceUuids.length(); i++) {
                    try {
                        String uuidStr = serviceUuids.getString(i);
                        UUID uuid = UUID.fromString(uuidStr);
                        dataBuilder.addServiceUuid(new ParcelUuid(uuid));
                    } catch (Exception e) {
                        Log.w(TAG, "Invalid service UUID at index " + i, e);
                    }
                }
            }

            // Start advertising
            bluetoothLeAdvertiser.startAdvertising(
                settingsBuilder.build(),
                dataBuilder.build(),
                advertiseCallback
            );

            Log.d(TAG, "Started advertising as: " + name);
            call.resolve();
        } catch (Exception e) {
            Log.e(TAG, "Start advertising failed", e);
            call.reject("Start advertising failed: " + e.getMessage());
        }
    }

    @PluginMethod
    public void stopAdvertising(PluginCall call) {
        if (!checkPermissions(call)) {
            return;
        }

        try {
            if (bluetoothLeAdvertiser != null) {
                bluetoothLeAdvertiser.stopAdvertising(advertiseCallback);
            }
            Log.d(TAG, "Stopped advertising");
            call.resolve();
        } catch (Exception e) {
            Log.e(TAG, "Stop advertising failed", e);
            call.reject("Stop advertising failed: " + e.getMessage());
        }
    }

    @PluginMethod
    public void addService(PluginCall call) {
        if (!checkPermissions(call)) {
            return;
        }

        try {
            String uuidStr = call.getString("uuid");
            if (uuidStr == null) {
                call.reject("Service UUID is required");
                return;
            }

            UUID uuid = UUID.fromString(uuidStr);
            boolean primary = call.getBoolean("primary", true);

            int serviceType = primary ? 
                BluetoothGattService.SERVICE_TYPE_PRIMARY : 
                BluetoothGattService.SERVICE_TYPE_SECONDARY;

            BluetoothGattService service = new BluetoothGattService(uuid, serviceType);
            services.put(uuidStr, service);

            // Initialize GATT server if not already done
            if (gattServer == null) {
                gattServer = bluetoothManager.openGattServer(getContext(), gattServerCallback);
            }

            // Add service to GATT server
            gattServer.addService(service);

            JSObject result = new JSObject();
            result.put("serviceId", uuidStr);
            Log.d(TAG, "Added service: " + uuidStr);
            call.resolve(result);
        } catch (Exception e) {
            Log.e(TAG, "Add service failed", e);
            call.reject("Add service failed: " + e.getMessage());
        }
    }

    @PluginMethod
    public void addCharacteristic(PluginCall call) {
        if (!checkPermissions(call)) {
            return;
        }

        try {
            String serviceUuid = call.getString("serviceUuid");
            String charUuid = call.getString("uuid");
            Integer properties = call.getInt("properties");
            Integer permissions = call.getInt("permissions");

            if (serviceUuid == null || charUuid == null || properties == null || permissions == null) {
                call.reject("Missing required parameters");
                return;
            }

            BluetoothGattService service = services.get(serviceUuid);
            if (service == null) {
                call.reject("Service not found: " + serviceUuid);
                return;
            }

            UUID uuid = UUID.fromString(charUuid);
            BluetoothGattCharacteristic characteristic = new BluetoothGattCharacteristic(
                uuid,
                properties,
                permissions
            );

            // Add initial value if provided
            String valueBase64 = call.getString("value");
            if (valueBase64 != null) {
                byte[] value = Base64.decode(valueBase64, Base64.DEFAULT);
                characteristic.setValue(value);
            }

            // Add CCCD descriptor for notifications/indications
            if ((properties & BluetoothGattCharacteristic.PROPERTY_NOTIFY) != 0 ||
                (properties & BluetoothGattCharacteristic.PROPERTY_INDICATE) != 0) {
                BluetoothGattDescriptor descriptor = new BluetoothGattDescriptor(
                    CLIENT_CHARACTERISTIC_CONFIG_UUID,
                    BluetoothGattDescriptor.PERMISSION_READ | BluetoothGattDescriptor.PERMISSION_WRITE
                );
                characteristic.addDescriptor(descriptor);
            }

            service.addCharacteristic(characteristic);
            characteristics.put(charUuid, characteristic);

            JSObject result = new JSObject();
            result.put("characteristicId", charUuid);
            Log.d(TAG, "Added characteristic: " + charUuid + " to service: " + serviceUuid);
            call.resolve(result);
        } catch (Exception e) {
            Log.e(TAG, "Add characteristic failed", e);
            call.reject("Add characteristic failed: " + e.getMessage());
        }
    }

    @PluginMethod
    public void updateCharacteristicValue(PluginCall call) {
        if (!checkPermissions(call)) {
            return;
        }

        try {
            String charUuid = call.getString("characteristicUuid");
            String valueBase64 = call.getString("value");

            if (charUuid == null || valueBase64 == null) {
                call.reject("Missing required parameters");
                return;
            }

            BluetoothGattCharacteristic characteristic = characteristics.get(charUuid);
            if (characteristic == null) {
                call.reject("Characteristic not found: " + charUuid);
                return;
            }

            byte[] value = Base64.decode(valueBase64, Base64.DEFAULT);
            characteristic.setValue(value);

            Log.d(TAG, "Updated characteristic value: " + charUuid);
            call.resolve();
        } catch (Exception e) {
            Log.e(TAG, "Update characteristic value failed", e);
            call.reject("Update characteristic value failed: " + e.getMessage());
        }
    }

    @PluginMethod
    public void sendNotification(PluginCall call) {
        if (!checkPermissions(call)) {
            return;
        }

        try {
            String charUuid = call.getString("characteristicUuid");
            String valueBase64 = call.getString("value");
            String deviceAddress = call.getString("deviceAddress");

            if (charUuid == null || valueBase64 == null) {
                call.reject("Missing required parameters");
                return;
            }

            BluetoothGattCharacteristic characteristic = characteristics.get(charUuid);
            if (characteristic == null) {
                call.reject("Characteristic not found: " + charUuid);
                return;
            }

            byte[] value = Base64.decode(valueBase64, Base64.DEFAULT);
            characteristic.setValue(value);

            // Get subscribed devices for this characteristic
            Set<BluetoothDevice> subscribers = notificationSubscriptions.get(charUuid);
            if (subscribers == null || subscribers.isEmpty()) {
                Log.w(TAG, "No subscribers for characteristic: " + charUuid);
                call.resolve();
                return;
            }

            // Send notification to specified device or all subscribers
            boolean sent = false;
            for (BluetoothDevice device : subscribers) {
                if (deviceAddress == null || device.getAddress().equals(deviceAddress)) {
                    boolean result = gattServer.notifyCharacteristicChanged(
                        device,
                        characteristic,
                        false
                    );
                    if (result) {
                        sent = true;
                        Log.d(TAG, "Sent notification to: " + device.getAddress());
                    }
                }
            }

            if (sent) {
                call.resolve();
            } else {
                call.reject("Failed to send notification");
            }
        } catch (Exception e) {
            Log.e(TAG, "Send notification failed", e);
            call.reject("Send notification failed: " + e.getMessage());
        }
    }

    @PluginMethod
    public void removeService(PluginCall call) {
        if (!checkPermissions(call)) {
            return;
        }

        try {
            String serviceId = call.getString("serviceId");
            if (serviceId == null) {
                call.reject("Service ID is required");
                return;
            }

            BluetoothGattService service = services.get(serviceId);
            if (service != null && gattServer != null) {
                gattServer.removeService(service);
                services.remove(serviceId);
                Log.d(TAG, "Removed service: " + serviceId);
            }

            call.resolve();
        } catch (Exception e) {
            Log.e(TAG, "Remove service failed", e);
            call.reject("Remove service failed: " + e.getMessage());
        }
    }

    @PluginMethod
    public void getConnectionState(PluginCall call) {
        JSObject result = new JSObject();
        result.put("isConnected", !connectedDevices.isEmpty());
        
        if (!connectedDevices.isEmpty()) {
            BluetoothDevice device = connectedDevices.iterator().next();
            result.put("deviceAddress", device.getAddress());
        }

        call.resolve(result);
    }

    public boolean checkPermissions(PluginCall call) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            if (ActivityCompat.checkSelfPermission(getContext(), 
                    Manifest.permission.BLUETOOTH_CONNECT) != PackageManager.PERMISSION_GRANTED ||
                ActivityCompat.checkSelfPermission(getContext(), 
                    Manifest.permission.BLUETOOTH_ADVERTISE) != PackageManager.PERMISSION_GRANTED) {
                call.reject("Bluetooth permissions not granted");
                return false;
            }
        }
        return true;
    }

    private final AdvertiseCallback advertiseCallback = new AdvertiseCallback() {
        @Override
        public void onStartSuccess(AdvertiseSettings settingsInEffect) {
            Log.d(TAG, "Advertising started successfully");
        }

        @Override
        public void onStartFailure(int errorCode) {
            Log.e(TAG, "Advertising failed with error code: " + errorCode);
        }
    };

    private final BluetoothGattServerCallback gattServerCallback = new BluetoothGattServerCallback() {
        @Override
        public void onConnectionStateChange(BluetoothDevice device, int status, int newState) {
            if (newState == BluetoothGatt.STATE_CONNECTED) {
                connectedDevices.add(device);
                Log.d(TAG, "Device connected: " + device.getAddress());
                
                JSObject data = new JSObject();
                data.put("isConnected", true);
                data.put("deviceAddress", device.getAddress());
                notifyListeners("connectionStateChange", data);
            } else if (newState == BluetoothGatt.STATE_DISCONNECTED) {
                connectedDevices.remove(device);
                // Remove from all notification subscriptions
                for (Set<BluetoothDevice> subscribers : notificationSubscriptions.values()) {
                    subscribers.remove(device);
                }
                Log.d(TAG, "Device disconnected: " + device.getAddress());
                
                JSObject data = new JSObject();
                data.put("isConnected", false);
                data.put("deviceAddress", device.getAddress());
                notifyListeners("connectionStateChange", data);
            }
        }

        @Override
        public void onCharacteristicReadRequest(BluetoothDevice device, int requestId, int offset,
                                                BluetoothGattCharacteristic characteristic) {
            Log.d(TAG, "Characteristic read request: " + characteristic.getUuid());
            
            JSObject data = new JSObject();
            data.put("serviceUuid", characteristic.getService().getUuid().toString());
            data.put("characteristicUuid", characteristic.getUuid().toString());
            data.put("deviceAddress", device.getAddress());
            notifyListeners("characteristicRead", data);

            if (ActivityCompat.checkSelfPermission(getContext(), 
                    Manifest.permission.BLUETOOTH_CONNECT) == PackageManager.PERMISSION_GRANTED) {
                gattServer.sendResponse(device, requestId, BluetoothGatt.GATT_SUCCESS, 
                    offset, characteristic.getValue());
            }
        }

        @Override
        public void onCharacteristicWriteRequest(BluetoothDevice device, int requestId,
                                                 BluetoothGattCharacteristic characteristic,
                                                 boolean preparedWrite, boolean responseNeeded,
                                                 int offset, byte[] value) {
            Log.d(TAG, "Characteristic write request: " + characteristic.getUuid());
            
            characteristic.setValue(value);
            
            JSObject data = new JSObject();
            data.put("serviceUuid", characteristic.getService().getUuid().toString());
            data.put("characteristicUuid", characteristic.getUuid().toString());
            data.put("value", Base64.encodeToString(value, Base64.NO_WRAP));
            data.put("deviceAddress", device.getAddress());
            notifyListeners("characteristicWrite", data);

            if (responseNeeded && ActivityCompat.checkSelfPermission(getContext(), 
                    Manifest.permission.BLUETOOTH_CONNECT) == PackageManager.PERMISSION_GRANTED) {
                gattServer.sendResponse(device, requestId, BluetoothGatt.GATT_SUCCESS, 
                    offset, value);
            }
        }

        @Override
        public void onDescriptorWriteRequest(BluetoothDevice device, int requestId,
                                            BluetoothGattDescriptor descriptor,
                                            boolean preparedWrite, boolean responseNeeded,
                                            int offset, byte[] value) {
            if (CLIENT_CHARACTERISTIC_CONFIG_UUID.equals(descriptor.getUuid())) {
                BluetoothGattCharacteristic characteristic = descriptor.getCharacteristic();
                String charUuid = characteristic.getUuid().toString();
                
                // Check if notifications are being enabled or disabled
                boolean notificationsEnabled = (value[0] & 0x01) != 0;
                
                if (notificationsEnabled) {
                    // Add device to notification subscriptions
                    Set<BluetoothDevice> subscribers = notificationSubscriptions.get(charUuid);
                    if (subscribers == null) {
                        subscribers = new HashSet<>();
                        notificationSubscriptions.put(charUuid, subscribers);
                    }
                    subscribers.add(device);
                    Log.d(TAG, "Notifications enabled for " + charUuid + " by " + device.getAddress());
                } else {
                    // Remove device from notification subscriptions
                    Set<BluetoothDevice> subscribers = notificationSubscriptions.get(charUuid);
                    if (subscribers != null) {
                        subscribers.remove(device);
                    }
                    Log.d(TAG, "Notifications disabled for " + charUuid + " by " + device.getAddress());
                }
            }

            if (responseNeeded && ActivityCompat.checkSelfPermission(getContext(), 
                    Manifest.permission.BLUETOOTH_CONNECT) == PackageManager.PERMISSION_GRANTED) {
                gattServer.sendResponse(device, requestId, BluetoothGatt.GATT_SUCCESS, 
                    offset, value);
            }
        }
    };

    @Override
    protected void handleOnDestroy() {
        if (gattServer != null) {
            gattServer.close();
            gattServer = null;
        }
        if (bluetoothLeAdvertiser != null) {
            try {
                bluetoothLeAdvertiser.stopAdvertising(advertiseCallback);
            } catch (Exception e) {
                Log.e(TAG, "Error stopping advertising", e);
            }
        }
        services.clear();
        characteristics.clear();
        connectedDevices.clear();
        notificationSubscriptions.clear();
    }
}
