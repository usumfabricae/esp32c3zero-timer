# BLE Peripheral Architecture

## Overview

This document describes the reversed BLE architecture where the Android app acts as a BLE peripheral (server) instead of a central (client), allowing ESP32 devices or other BLE central devices to connect to the phone.

## Architecture Change

### Original Architecture
```
ESP32 Device (BLE Server/Peripheral)
         ↓
    BLE Connection
         ↓
Android App (BLE Client/Central)
```

### New Architecture
```
Android App (BLE Server/Peripheral)
         ↓
    BLE Connection
         ↓
ESP32 Device (BLE Client/Central)
```

## Why This Change?

1. **Mobile-First Control**: The phone becomes the primary controller, storing schedules and settings
2. **Multiple Device Support**: One phone can manage multiple ESP32 devices
3. **Offline Capability**: Phone retains all configuration even when devices are disconnected
4. **Background Operation**: Android can advertise in the background (with limitations)
5. **Centralized Management**: All device state lives on the phone

## Implementation

### Custom Capacitor Plugin

A new Capacitor plugin (`@superfede/capacitor-ble-peripheral`) provides BLE peripheral functionality:

**Location**: `webclient/ble-peripheral-plugin/`

**Key Features**:
- Start/stop BLE advertising
- Create GATT services and characteristics
- Handle read/write requests from connected clients
- Send notifications to subscribed clients
- Monitor connection state

**Platform Support**:
- ✅ Android (API 24+)
- ❌ iOS (not supported by iOS)
- ❌ Web (Web Bluetooth API only supports central role)

### Plugin Structure

```
ble-peripheral-plugin/
├── package.json                    # Plugin metadata
├── tsconfig.json                   # TypeScript configuration
├── README.md                       # Plugin documentation
├── src/
│   ├── definitions.ts              # TypeScript interfaces
│   ├── index.ts                    # Plugin registration
│   └── web.ts                      # Web stub (unsupported)
└── android/
    ├── build.gradle                # Android build config
    ├── src/main/
    │   ├── AndroidManifest.xml     # Permissions
    │   └── java/net/superfede/capacitor/bleperipheral/
    │       └── BlePeripheralPlugin.java  # Android implementation
```

### React Hook

**Location**: `webclient/src/hooks/useBLEPeripheral.js`

The `useBLEPeripheral` hook provides a React-friendly interface to the plugin:

```javascript
const {
  isAdvertising,
  isConnected,
  connectedDevice,
  deviceState,
  initialize,
  startAdvertising,
  stopAdvertising,
  updateTemperature,
  updateCurrentTime,
  updateBatteryLevel
} = useBLEPeripheral();
```

## GATT Services & Characteristics

The Android app exposes the same GATT profile as the ESP32 device:

### Main Service (0x00FF)
- **Temperature** (0x2A6E): READ, NOTIFY
- **Current Time** (0x2A2B): READ, NOTIFY
- **Relay State** (0xFF02): READ, WRITE
- **Schedule** (0xFF05): READ, WRITE
- **Temperature Thresholds** (0xFF06): READ, WRITE
- **Temperature Calibration** (0xFF07): WRITE
- **WiFi SSID** (0xFF08): READ, WRITE
- **WiFi Password** (0xFF09): WRITE
- **BLE Passkey** (0xFF0A): WRITE

### Battery Service (0x180F)
- **Battery Level** (0x2A19): READ, NOTIFY

## Usage Example

### Initialize and Start Advertising

```javascript
import { useBLEPeripheral } from './hooks/useBLEPeripheral';

function App() {
  const peripheral = useBLEPeripheral();

  useEffect(() => {
    const setup = async () => {
      await peripheral.initialize();
      await peripheral.startAdvertising('MyTimer');
    };
    setup();
  }, []);

  return (
    <div>
      <p>Advertising: {peripheral.isAdvertising ? 'Yes' : 'No'}</p>
      <p>Connected: {peripheral.isConnected ? 'Yes' : 'No'}</p>
      {peripheral.connectedDevice && (
        <p>Device: {peripheral.connectedDevice}</p>
      )}
    </div>
  );
}
```

### Update Values and Send Notifications

```javascript
// Update temperature (automatically notifies connected clients)
await peripheral.updateTemperature(22.5);

// Update battery level
await peripheral.updateBatteryLevel(85);

// Update current time
await peripheral.updateCurrentTime(new Date());
```

### Handle Incoming Writes

The hook automatically handles characteristic writes from connected clients:

```javascript
// When a client writes to relay state characteristic
// The hook updates deviceState.relayState automatically

// Access current state
console.log('Current relay state:', peripheral.deviceState.relayState);
console.log('Temperature thresholds:', peripheral.deviceState.thresholds);
```

## Installation

### 1. Install the Plugin

```bash
cd webclient
npm install ./ble-peripheral-plugin
```

### 2. Sync with Capacitor

```bash
npx cap sync android
```

### 3. Update Android Permissions

The plugin automatically adds required permissions, but ensure your app's `AndroidManifest.xml` includes:

```xml
<uses-permission android:name="android.permission.BLUETOOTH" />
<uses-permission android:name="android.permission.BLUETOOTH_ADMIN" />
<uses-permission android:name="android.permission.BLUETOOTH_ADVERTISE" />
<uses-permission android:name="android.permission.BLUETOOTH_CONNECT" />
```

### 4. Request Runtime Permissions

For Android 12+, request permissions at runtime:

```javascript
import { Capacitor } from '@capacitor/core';

if (Capacitor.getPlatform() === 'android') {
  // Request BLUETOOTH_ADVERTISE and BLUETOOTH_CONNECT permissions
  // Use Capacitor Permissions API or native Android code
}
```

## ESP32 Client Implementation

To connect from ESP32 to the Android peripheral, you'll need to modify the ESP32 firmware to act as a BLE client instead of a server. This involves:

1. **Scanning for devices** with the advertised name
2. **Connecting to the Android peripheral**
3. **Discovering services and characteristics**
4. **Reading/writing characteristics**
5. **Subscribing to notifications**

See ESP-IDF BLE client examples for implementation details.

## Limitations

### Android Background Advertising

Android has restrictions on background BLE advertising:
- Limited advertising time when app is in background
- May require foreground service notification
- Battery optimization settings can affect advertising

### iOS Not Supported

iOS does not allow apps to act as BLE peripherals in the background. The Web Bluetooth API also only supports the central role.

### Connection Management

- Only one client can connect at a time (configurable)
- Connection state must be monitored
- Reconnection logic needed for reliability

## Testing

### Test with nRF Connect

Use Nordic's nRF Connect app to test the peripheral:

1. Start advertising from your app
2. Open nRF Connect on another device
3. Scan for devices
4. Connect to your advertised device name
5. Explore services and characteristics
6. Read/write values and subscribe to notifications

### Test with ESP32

Flash ESP32 with BLE client firmware that:
1. Scans for your device name
2. Connects and discovers services
3. Reads temperature and battery
4. Writes relay state
5. Subscribes to notifications

## Future Enhancements

1. **Multiple Connections**: Support multiple connected clients
2. **Security**: Implement pairing and encryption
3. **Persistence**: Save device state to local storage
4. **Sync**: Sync state with cloud backend
5. **Foreground Service**: Keep advertising in background with notification

## References

- [Android BLE Peripheral Documentation](https://developer.android.com/guide/topics/connectivity/bluetooth/ble-overview)
- [Capacitor Plugin Development](https://capacitorjs.com/docs/plugins)
- [ESP-IDF BLE Client Examples](https://github.com/espressif/esp-idf/tree/master/examples/bluetooth/bluedroid/ble)
