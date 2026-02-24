# Installing BLE Peripheral Plugin

Quick guide to install and use the custom BLE peripheral plugin.

## Installation Steps

### 1. Install Plugin Dependencies

```bash
cd webclient/ble-peripheral-plugin
npm install
cd ..
```

### 2. Install Plugin in Web Client

```bash
cd webclient
npm install ./ble-peripheral-plugin
```

### 3. Update Capacitor Configuration

The plugin should be automatically detected. Verify in `capacitor.config.json`:

```json
{
  "plugins": {
    "BlePeripheral": {}
  }
}
```

### 4. Sync with Android

```bash
npx cap sync android
```

### 5. Update Android Manifest (if needed)

The plugin includes permissions, but verify in `android/app/src/main/AndroidManifest.xml`:

```xml
<uses-permission android:name="android.permission.BLUETOOTH" />
<uses-permission android:name="android.permission.BLUETOOTH_ADMIN" />
<uses-permission android:name="android.permission.BLUETOOTH_ADVERTISE" />
<uses-permission android:name="android.permission.BLUETOOTH_CONNECT" />
<uses-feature android:name="android.hardware.bluetooth_le" android:required="true" />
```

### 6. Build Android App

```bash
cd android
./gradlew assembleDebug
```

Or open in Android Studio:

```bash
npx cap open android
```

## Usage in React Components

### Import the Hook

```javascript
import { useBLEPeripheral } from './hooks/useBLEPeripheral';
```

### Use in Component

```javascript
function MyComponent() {
  const peripheral = useBLEPeripheral();

  useEffect(() => {
    const init = async () => {
      const success = await peripheral.initialize();
      if (success) {
        await peripheral.startAdvertising('ESP32C3_Timer');
      }
    };
    init();
  }, []);

  return (
    <div>
      <h2>BLE Peripheral Status</h2>
      <p>Advertising: {peripheral.isAdvertising ? '✓' : '✗'}</p>
      <p>Connected: {peripheral.isConnected ? '✓' : '✗'}</p>
      {peripheral.connectedDevice && (
        <p>Device: {peripheral.connectedDevice}</p>
      )}
      
      <button onClick={() => peripheral.updateTemperature(25.5)}>
        Update Temperature
      </button>
      
      <button onClick={() => peripheral.updateBatteryLevel(80)}>
        Update Battery
      </button>
    </div>
  );
}
```

## Testing

### Test with nRF Connect App

1. Install nRF Connect on another Android device
2. Run your app and start advertising
3. Open nRF Connect and scan for devices
4. Look for "ESP32C3_Timer" in the list
5. Connect and explore services
6. Try reading/writing characteristics

### Test with ESP32 (Future)

You'll need to modify the ESP32 firmware to act as a BLE client:

1. Scan for the advertised device name
2. Connect to the Android peripheral
3. Discover services and characteristics
4. Read/write values
5. Subscribe to notifications

## Troubleshooting

### Plugin Not Found

```bash
# Reinstall plugin
cd webclient
npm uninstall @superfede/capacitor-ble-peripheral
npm install ./ble-peripheral-plugin
npx cap sync android
```

### Permissions Denied

Check Android settings:
- Bluetooth is enabled
- Location permission granted (required for BLE on Android)
- App has Bluetooth permissions

### Advertising Fails

- Ensure Bluetooth adapter supports peripheral mode
- Check that another app isn't already advertising
- Verify device supports BLE advertising

### Build Errors

```bash
# Clean and rebuild
cd android
./gradlew clean
./gradlew assembleDebug
```

## Next Steps

1. **Integrate into existing UI**: Add peripheral controls to your Dashboard
2. **Implement state management**: Sync device state with UI
3. **Add persistence**: Save state to local storage
4. **Handle reconnection**: Implement automatic reconnection logic
5. **Modify ESP32**: Update firmware to act as BLE client

## Documentation

- Plugin README: `webclient/ble-peripheral-plugin/README.md`
- Architecture: `BLE_PERIPHERAL_ARCHITECTURE.md`
- Hook implementation: `webclient/src/hooks/useBLEPeripheral.js`
