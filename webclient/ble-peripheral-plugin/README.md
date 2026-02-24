# Capacitor BLE Peripheral Plugin

A Capacitor plugin that enables Android devices to act as BLE peripherals (servers), allowing them to advertise services and characteristics that can be discovered and connected to by BLE central devices.

## Features

- Start/stop BLE advertising
- Create GATT services and characteristics
- Handle read/write requests from connected clients
- Send notifications to subscribed clients
- Monitor connection state
- Support for multiple connected devices

## Installation

### Local Development

Since this is a local plugin, install it from the local directory:

```bash
cd webclient
npm install ./ble-peripheral-plugin
```

### Add to Capacitor

```bash
npx cap sync android
```

## Android Permissions

The plugin automatically requests the following permissions:

- `BLUETOOTH`
- `BLUETOOTH_ADMIN`
- `BLUETOOTH_ADVERTISE` (Android 12+)
- `BLUETOOTH_CONNECT` (Android 12+)

These are declared in the plugin's AndroidManifest.xml and will be merged with your app's manifest.

## Usage

### Initialize the Plugin

```javascript
import { BlePeripheral } from '@superfede/capacitor-ble-peripheral';

await BlePeripheral.initialize();
```

### Start Advertising

```javascript
await BlePeripheral.startAdvertising({
  name: 'ESP32C3_Timer',
  serviceUuids: ['000000ff-0000-1000-8000-00805f9b34fb'],
  connectable: true,
  timeout: 0 // 0 = no timeout
});
```

### Add a Service

```javascript
const { serviceId } = await BlePeripheral.addService({
  uuid: '000000ff-0000-1000-8000-00805f9b34fb',
  primary: true
});
```

### Add a Characteristic

```javascript
import { BleProperty, BlePermission } from '@superfede/capacitor-ble-peripheral';

const { characteristicId } = await BlePeripheral.addCharacteristic({
  serviceUuid: '000000ff-0000-1000-8000-00805f9b34fb',
  uuid: '00002a6e-0000-1000-8000-00805f9b34fb',
  properties: BleProperty.READ | BleProperty.NOTIFY,
  permissions: BlePermission.READ,
  value: btoa('initial value') // base64 encoded
});
```

### Update Characteristic Value

```javascript
await BlePeripheral.updateCharacteristicValue({
  serviceUuid: '000000ff-0000-1000-8000-00805f9b34fb',
  characteristicUuid: '00002a6e-0000-1000-8000-00805f9b34fb',
  value: btoa('new value') // base64 encoded
});
```

### Send Notification

```javascript
await BlePeripheral.sendNotification({
  serviceUuid: '000000ff-0000-1000-8000-00805f9b34fb',
  characteristicUuid: '00002a6e-0000-1000-8000-00805f9b34fb',
  value: btoa('notification data') // base64 encoded
});
```

### Listen for Events

```javascript
// Listen for characteristic writes
await BlePeripheral.addListener('characteristicWrite', (data) => {
  console.log('Write received:', {
    serviceUuid: data.serviceUuid,
    characteristicUuid: data.characteristicUuid,
    value: atob(data.value), // decode base64
    deviceAddress: data.deviceAddress
  });
});

// Listen for characteristic reads
await BlePeripheral.addListener('characteristicRead', (data) => {
  console.log('Read request:', data);
});

// Listen for connection state changes
await BlePeripheral.addListener('connectionStateChange', (data) => {
  console.log('Connection state:', data.isConnected);
  if (data.deviceAddress) {
    console.log('Device:', data.deviceAddress);
  }
});
```

### Stop Advertising

```javascript
await BlePeripheral.stopAdvertising();
```

## Property and Permission Constants

### BleProperty

- `READ` (0x02) - Characteristic can be read
- `WRITE` (0x08) - Characteristic can be written with response
- `WRITE_NO_RESPONSE` (0x04) - Characteristic can be written without response
- `NOTIFY` (0x10) - Characteristic supports notifications
- `INDICATE` (0x20) - Characteristic supports indications

### BlePermission

- `READ` (0x01) - Read permission
- `WRITE` (0x10) - Write permission

Combine properties/permissions using bitwise OR:

```javascript
properties: BleProperty.READ | BleProperty.WRITE | BleProperty.NOTIFY
```

## Platform Support

- **Android**: Fully supported (API 24+)
- **iOS**: Not supported (iOS does not allow apps to act as BLE peripherals in the background)
- **Web**: Not supported (Web Bluetooth API only supports central role)

## Example: ESP32 Timer as Android Peripheral

See `webclient/src/hooks/useBLEPeripheral.js` for a complete implementation that mirrors the ESP32 device's BLE server functionality.

## API Reference

See [definitions.ts](src/definitions.ts) for complete API documentation.

## License

MIT
