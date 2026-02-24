# BLE Peripheral Demo - Quick Start

## Overview

The BLE Peripheral Demo is a simplified Android app that demonstrates the reversed BLE architecture where the phone acts as a BLE server instead of a client.

## What's Different?

**Original App**: Phone connects TO ESP32 device (phone = client, ESP32 = server)  
**Peripheral Demo**: ESP32 connects TO phone (phone = server, ESP32 = client)

## Quick Build via Codemagic

### Automatic Build

Push to the `main` or `peripheral-demo` branch:

```bash
git add .
git commit -m "Build peripheral demo"
git push origin main
```

The `android-peripheral-demo` workflow will automatically:
1. Build the peripheral demo web app
2. Create Android APK
3. Upload as artifact

### Download APK

1. Go to [Codemagic Dashboard](https://codemagic.io)
2. Find the `android-peripheral-demo` build
3. Download `ble-peripheral-demo-debug.apk` from artifacts
4. Install on Android device

## Local Build

### One-Command Build (Windows)

```powershell
cd webclient
.\build-peripheral-demo.ps1
```

APK will be at: `webclient/ble-peripheral-demo.apk`

### Manual Build

```bash
cd webclient
npm install
npm install ./ble-peripheral-plugin
npm run build:peripheral
npx cap add android --capacitor-config capacitor-peripheral.config.json
mv android android-peripheral
npx cap sync android --capacitor-config capacitor-peripheral.config.json
cd android-peripheral
./gradlew assembleDebug
```

## Testing

### 1. Install APK

```bash
adb install ble-peripheral-demo.apk
```

### 2. Open App

- Launch "BLE Peripheral Demo" on Android device
- Click "Start Advertising"
- App will advertise as "ESP32C3_Timer"

### 3. Test with nRF Connect

On another Android device:
1. Install nRF Connect app
2. Scan for devices
3. Connect to "ESP32C3_Timer"
4. Explore services (0x00FF, 0x180F)
5. Read characteristics (temperature, battery, etc.)
6. Subscribe to notifications
7. Write to characteristics (relay state, thresholds)

### 4. Test Features

In the peripheral demo app:
- **Update Temperature**: Click to change temp and send notification
- **Update Battery**: Click to change battery level
- **Update Time**: Click to update current time
- **Monitor Connection**: See when nRF Connect connects/disconnects

## Project Files

### New Files Created

```
webclient/
├── src/
│   ├── AppPeripheralDemo.jsx           # Peripheral demo app
│   ├── main-peripheral.jsx             # Entry point
│   └── hooks/
│       └── useBLEPeripheral.js         # Peripheral hook
├── ble-peripheral-plugin/              # Custom Capacitor plugin
│   ├── src/
│   │   ├── definitions.ts              # TypeScript interfaces
│   │   ├── index.ts                    # Plugin registration
│   │   └── web.ts                      # Web stub
│   └── android/
│       └── src/main/java/.../BlePeripheralPlugin.java
├── index-peripheral.html               # HTML template
├── vite.config.peripheral.js           # Vite config
├── capacitor-peripheral.config.json    # Capacitor config
├── build-peripheral-demo.ps1           # Build script
└── PERIPHERAL_DEMO_BUILD.md            # Detailed guide
```

### Updated Files

```
codemagic.yaml                          # Added android-peripheral-demo workflow
webclient/package.json                  # Added build:peripheral script
webclient/.gitignore                    # Added dist-peripheral, android-peripheral
```

### Documentation

```
BLE_PERIPHERAL_ARCHITECTURE.md          # Architecture explanation
BLE_PERIPHERAL_SUMMARY.md               # Complete overview
INSTALL_BLE_PERIPHERAL.md               # Installation guide
PERIPHERAL_DEMO_QUICK_START.md          # This file
```

## Codemagic Workflow

The `android-peripheral-demo` workflow in `codemagic.yaml`:

```yaml
android-peripheral-demo:
  name: Android BLE Peripheral Demo
  environment:
    vars:
      PACKAGE_NAME: "net.superfede.BlePeripheralDemo"
      APP_NAME: "BLE Peripheral Demo"
    node: 22
    java: 21
  triggering:
    branch_patterns:
      - pattern: 'main'
      - pattern: 'peripheral-demo'
```

## Key Features

### BLE Peripheral Plugin

- Start/stop advertising
- Create GATT services and characteristics
- Handle read/write requests
- Send notifications
- Monitor connections

### Demo App

- Simple UI showing peripheral status
- Connection monitoring
- Device state display (temp, battery, relay)
- Update buttons to test notifications
- Instructions for testing

## Architecture

```
┌─────────────────────┐
│   Android Phone     │
│  (BLE Peripheral)   │ ← Advertises as "ESP32C3_Timer"
│                     │ ← Exposes GATT services
└──────────┬──────────┘
           │
           │ BLE Connection
           │
           ↓
┌─────────────────────┐
│   nRF Connect       │
│  (BLE Central)      │ ← Scans and connects
│                     │ ← Reads/writes characteristics
└─────────────────────┘
```

## Next Steps

1. **Build APK** using Codemagic or local script
2. **Install** on Android device
3. **Test** with nRF Connect app
4. **Verify** all BLE features work
5. **Modify ESP32** firmware to act as BLE client
6. **Connect ESP32** to Android peripheral

## Troubleshooting

### Build Issues

**Codemagic build fails**
- Check workflow logs in Codemagic dashboard
- Verify all files are committed and pushed
- Check Node.js and Java versions match requirements

**Local build fails**
- Run `npm install` to ensure dependencies are installed
- Check that Java 21 and Android SDK are installed
- Try cleaning: `rm -rf node_modules && npm install`

### Runtime Issues

**App crashes on startup**
- Check Android version (requires API 24+)
- View logs: `adb logcat | grep BlePeripheral`

**Advertising fails**
- Enable Bluetooth on device
- Grant Bluetooth permissions in Settings
- Check device supports BLE peripheral mode

**Can't connect from nRF Connect**
- Verify advertising is active (green checkmark)
- Check device name is "ESP32C3_Timer"
- Try restarting Bluetooth on both devices

## Resources

- **Detailed Build Guide**: `webclient/PERIPHERAL_DEMO_BUILD.md`
- **Plugin Documentation**: `webclient/ble-peripheral-plugin/README.md`
- **Architecture Details**: `BLE_PERIPHERAL_ARCHITECTURE.md`
- **Installation Guide**: `INSTALL_BLE_PERIPHERAL.md`
- **Complete Summary**: `BLE_PERIPHERAL_SUMMARY.md`

## Support

For issues:
1. Check documentation files listed above
2. Review Codemagic build logs
3. Check Android logcat output
4. Test with nRF Connect to isolate issues
