# BLE Configuration Characteristics

This document describes the new BLE characteristics added for WiFi and security configuration.

## New Characteristics

### 1. WiFi SSID (0xFF08)
- **UUID**: 0xFF08
- **Permissions**: Read + Write
- **Max Length**: 32 bytes
- **Format**: UTF-8 string
- **Description**: WiFi network name (SSID)

**Read**: Returns the currently configured WiFi SSID from NVS (or default from config.h if not set)

**Write**: Updates the WiFi SSID in NVS
- Send: ASCII string (1-32 bytes)
- Example: "MyHomeNetwork"
- Logged in serial output for debugging

### 2. WiFi Password (0xFF09)
- **UUID**: 0xFF09
- **Permissions**: Write only
- **Max Length**: 64 bytes
- **Format**: UTF-8 string
- **Description**: WiFi network password

**Write**: Updates the WiFi password in NVS
- Send: ASCII string (1-64 bytes)
- Example: "MySecurePassword123"
- **Security Note**: Password is logged in serial output for debugging purposes
- Logged in serial output: "WiFi Password updated: [password]"

### 3. BLE Passkey (0xFF0A)
- **UUID**: 0xFF0A
- **Permissions**: Write only
- **Max Length**: 6 bytes
- **Format**: 6-digit numeric string
- **Description**: BLE pairing passkey

**Write**: Updates the BLE pairing passkey in NVS
- Send: 6-digit ASCII string (e.g., "123456")
- Valid range: 000000 to 999999
- Takes effect on next pairing attempt
- **Security Note**: Passkey is logged in serial output for debugging purposes
- Logged in serial output: "BLE Passkey updated: [passkey]"

## Usage Examples

### Reading WiFi SSID
```javascript
// Web Bluetooth API example
const value = await wifiSsidCharacteristic.readValue();
const ssid = new TextDecoder().decode(value);
console.log('Current SSID:', ssid);
```

### Writing WiFi SSID
```javascript
// Web Bluetooth API example
const encoder = new TextEncoder();
const ssid = "MyNewNetwork";
await wifiSsidCharacteristic.writeValue(encoder.encode(ssid));
```

### Writing WiFi Password
```javascript
// Web Bluetooth API example
const encoder = new TextEncoder();
const password = "MyNewPassword123";
await wifiPasswordCharacteristic.writeValue(encoder.encode(password));
```

### Writing BLE Passkey
```javascript
// Web Bluetooth API example
const encoder = new TextEncoder();
const passkey = "654321";  // Must be exactly 6 digits
await blePasskeyCharacteristic.writeValue(encoder.encode(passkey));
```

## NVS Storage

All configuration values are stored in NVS (Non-Volatile Storage):

- **WiFi SSID**: Namespace "wifi_config", Key "ssid"
- **WiFi Password**: Namespace "wifi_config", Key "password"
- **BLE Passkey**: Namespace "ble_security", Key "passkey"

## Security Considerations

1. **WiFi Password**: Stored in NVS and logged to serial output. Ensure serial monitor is not accessible in production.

2. **BLE Passkey**: 
   - Stored in NVS and logged to serial output
   - Takes effect on next pairing (not immediately)
   - Device must be unpaired and re-paired for new passkey to be used
   - Logged during pairing process for debugging

3. **BLE Security**: All characteristics require authenticated BLE connection with MITM protection

## Default Values

If no values are stored in NVS, the system uses defaults from `config.h`:
- WiFi SSID: `WIFI_SSID`
- WiFi Password: `WIFI_PASS`
- BLE Passkey: `DEFAULT_PASSKEY` (123456)

## Reboot Requirement

After changing WiFi credentials, the device should be rebooted for changes to take effect:
- WiFi connection is established during boot
- New credentials will be used on next boot/wake cycle
