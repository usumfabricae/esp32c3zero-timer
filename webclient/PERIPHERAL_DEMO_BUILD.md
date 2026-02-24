# BLE Peripheral Demo Build Guide

This guide explains how to build the BLE Peripheral Demo APK both locally and via Codemagic CI/CD.

## What is the Peripheral Demo?

A simplified Android app that demonstrates BLE peripheral (server) functionality. The app:
- Acts as a BLE server (peripheral)
- Advertises as "ESP32C3_Timer"
- Exposes GATT services and characteristics
- Allows testing with nRF Connect or other BLE clients
- Shows connection status and device state
- Sends notifications to connected clients

## Local Build

### Prerequisites

- Node.js 22+
- Java 21
- Android SDK
- PowerShell (Windows) or Bash (Linux/Mac)

### Build Steps

#### Windows

```powershell
cd webclient
.\build-peripheral-demo.ps1
```

#### Linux/Mac

```bash
cd webclient
chmod +x build-peripheral-demo.ps1
./build-peripheral-demo.ps1
```

### Manual Build Steps

If the script doesn't work, follow these steps:

```bash
# 1. Install dependencies
cd webclient
npm install

# 2. Install BLE peripheral plugin
npm install ./ble-peripheral-plugin

# 3. Build web app
npm run build:peripheral

# 4. Add Android platform (first time only)
npx cap add android --capacitor-config capacitor-peripheral.config.json
mv android android-peripheral

# 5. Sync Capacitor
npx cap sync android --capacitor-config capacitor-peripheral.config.json

# 6. Build APK
cd android-peripheral
./gradlew assembleDebug

# APK will be at: app/build/outputs/apk/debug/app-debug.apk
```

## Codemagic CI/CD Build

### Automatic Builds

The `android-peripheral-demo` workflow in `codemagic.yaml` automatically builds the peripheral demo APK when:
- Code is pushed to `main` branch
- Code is pushed to `peripheral-demo` branch
- A pull request is created

### Workflow Configuration

The workflow:
1. Installs npm dependencies
2. Installs the BLE peripheral plugin
3. Builds the peripheral demo web app
4. Initializes Capacitor with peripheral config
5. Adds Android platform
6. Syncs Capacitor
7. Builds the debug APK
8. Renames APK to `ble-peripheral-demo-debug.apk`
9. Uploads as artifact

### Triggering a Build

**Option 1: Push to main**
```bash
git add .
git commit -m "Update peripheral demo"
git push origin main
```

**Option 2: Create peripheral-demo branch**
```bash
git checkout -b peripheral-demo
git push origin peripheral-demo
```

**Option 3: Manual trigger**
- Go to Codemagic dashboard
- Select the `android-peripheral-demo` workflow
- Click "Start new build"

### Download APK

After the build completes:
1. Go to Codemagic dashboard
2. Click on the completed build
3. Download the APK from artifacts section
4. Install on Android device

## Testing the APK

### Install on Android Device

```bash
# Via ADB
adb install ble-peripheral-demo.apk

# Or transfer to device and install manually
```

### Test with nRF Connect

1. Install nRF Connect app on another Android device
2. Open the peripheral demo app
3. Click "Start Advertising"
4. Open nRF Connect and scan for devices
5. Look for "ESP32C3_Timer" in the list
6. Connect to the device
7. Explore services and characteristics
8. Subscribe to notifications
9. Try reading/writing values

### Test Features

- **Advertising**: Start/stop advertising
- **Connection**: Monitor connection state
- **Device State**: View temperature, battery, relay state
- **Updates**: Click update buttons to change values
- **Notifications**: Connected clients receive notifications

## Project Structure

```
webclient/
├── src/
│   ├── AppPeripheralDemo.jsx       # Peripheral demo app
│   ├── main-peripheral.jsx         # Peripheral entry point
│   ├── hooks/
│   │   └── useBLEPeripheral.js     # Peripheral hook
│   └── components/
│       ├── BLEPeripheralDemo.jsx   # Demo component
│       └── BLEPeripheralDemo.css   # Demo styles
├── ble-peripheral-plugin/          # Custom plugin
├── index-peripheral.html           # Peripheral HTML
├── vite.config.peripheral.js       # Peripheral Vite config
├── capacitor-peripheral.config.json # Peripheral Capacitor config
├── build-peripheral-demo.ps1       # Build script
└── android-peripheral/             # Android project (generated)
```

## Configuration Files

### capacitor-peripheral.config.json

```json
{
  "appId": "net.superfede.BlePeripheralDemo",
  "appName": "BLE Peripheral Demo",
  "webDir": "dist-peripheral",
  "plugins": {
    "BlePeripheral": {}
  }
}
```

### vite.config.peripheral.js

Builds the app using `main-peripheral.jsx` as entry point and outputs to `dist-peripheral/`.

## Troubleshooting

### Build Fails

**Problem**: npm install fails
```bash
# Clear cache and reinstall
rm -rf node_modules package-lock.json
npm install
```

**Problem**: Gradle build fails
```bash
# Clean and rebuild
cd android-peripheral
./gradlew clean
./gradlew assembleDebug
```

**Problem**: Plugin not found
```bash
# Reinstall plugin
npm uninstall @superfede/capacitor-ble-peripheral
npm install ./ble-peripheral-plugin
npx cap sync android --capacitor-config capacitor-peripheral.config.json
```

### Runtime Issues

**Problem**: App crashes on startup
- Check Android version (requires API 24+)
- Check logcat for errors: `adb logcat | grep BlePeripheral`

**Problem**: Advertising fails
- Ensure Bluetooth is enabled
- Grant Bluetooth permissions in app settings
- Check that device supports BLE peripheral mode

**Problem**: Can't connect from nRF Connect
- Verify advertising is active
- Check device name matches "ESP32C3_Timer"
- Ensure both devices have Bluetooth enabled

## Codemagic Configuration

The workflow is defined in `/codemagic.yaml`:

```yaml
android-peripheral-demo:
  name: Android BLE Peripheral Demo
  environment:
    vars:
      PACKAGE_NAME: "net.superfede.BlePeripheralDemo"
      APP_NAME: "BLE Peripheral Demo"
    node: 22
    java: 21
```

Key steps:
1. Install dependencies
2. Install BLE peripheral plugin
3. Build peripheral demo web app
4. Initialize Capacitor with peripheral config
5. Add Android platform
6. Sync Capacitor
7. Build APK
8. Upload artifact

## Next Steps

After building and testing:
1. Test all BLE features with nRF Connect
2. Verify notifications work correctly
3. Test read/write operations
4. Check connection stability
5. Modify ESP32 firmware to act as BLE client
6. Test ESP32 connecting to Android peripheral

## Resources

- Plugin README: `ble-peripheral-plugin/README.md`
- Architecture: `../BLE_PERIPHERAL_ARCHITECTURE.md`
- Installation: `../INSTALL_BLE_PERIPHERAL.md`
- Summary: `../BLE_PERIPHERAL_SUMMARY.md`
