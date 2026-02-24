# Android App - ESP32-C3 Timer

Native Android application for controlling the ESP32-C3 temperature-controlled relay timer via Bluetooth Low Energy.

## Overview

The Android app is built using Capacitor, which wraps the React web application in a native Android container. It provides the same functionality as the web version but uses the Capacitor Bluetooth LE plugin instead of the Web Bluetooth API.

## Features

- Native Android app experience
- Full BLE functionality (same as web version)
- Automatic platform detection (no code changes needed)
- Offline installation (no browser required)
- Better Bluetooth permissions handling
- Runs on Android 7.0+ (API 24+)

## Architecture

### Platform Abstraction

The app uses a unified BLE interface that automatically detects the platform:

```
App.jsx
  └─> useBLEUnified.js (detects platform)
       ├─> useBLE.js (Web Bluetooth API - for web)
       └─> useBLECapacitor.js (Capacitor BLE - for Android)
```

**Key Files:**
- `src/hooks/useBLEUnified.js` - Platform detection and routing
- `src/hooks/useBLE.js` - Web Bluetooth API implementation
- `src/hooks/useBLECapacitor.js` - Capacitor BLE implementation
- `src/utils/dataFormatter.js` - Shared data conversion utilities

### Build Configuration

**Conditional Base Path:**
The Vite configuration uses environment variables to set the correct base path:
- Web build: `base: '/esp32c3zero-timer/'` (for GitHub Pages)
- Android build: `base: '/'` (for local file access)

Set via: `CAPACITOR=true npm run build`

## Requirements

### Development
- Node.js v14+
- npm or yarn
- Android Studio (for native development)
- Java 21 (for Gradle builds)

### Runtime
- Android 7.0+ (API 24+)
- Bluetooth Low Energy support
- Location permissions (required for BLE scanning on Android)

## Installation

### For Users

1. Download the APK from Codemagic artifacts
2. Enable "Install from unknown sources" in Android settings
3. Install the APK
4. Grant Bluetooth and Location permissions when prompted
5. Open the app and connect to your ESP32 device

### For Developers

```bash
# Clone repository
git clone <repository-url>
cd esp32c3zero-timer/webclient

# Install dependencies
npm install

# Build for Android
CAPACITOR=true npm run build

# Sync to Android project
npx cap sync android

# Open in Android Studio
npx cap open android

# Or build directly
cd android
./gradlew assembleDebug
```

## Building

### Local Build

```bash
cd webclient

# Build web assets for Android
CAPACITOR=true npm run build

# Sync to Android
npx cap sync android

# Build APK
cd android
./gradlew assembleDebug
```

Output: `android/app/build/outputs/apk/debug/app-debug.apk`

### CI/CD Build (Codemagic)

The project includes automated builds via Codemagic:

**Configuration:** `webclient/codemagic.yaml`

**Triggers:**
- Push to `main` branch
- Pull requests
- Git tags

**Build Steps:**
1. Install npm dependencies
2. Check for existing Capacitor config (skip init if exists)
3. Add Android platform (if not exists)
4. Build web app with `CAPACITOR=true`
5. Sync Capacitor
6. Build Android debug APK

**Artifacts:**
- APK files available in Codemagic build artifacts
- Email notifications on build success/failure

## Configuration

### Capacitor Config

**File:** `webclient/capacitor.config.json`

```json
{
  "appId": "net.superfede.BleTimerWebView",
  "appName": "BLE Timer",
  "webDir": "dist",
  "server": {
    "androidScheme": "https"
  },
  "android": {
    "buildOptions": {
      "releaseType": "APK"
    }
  },
  "plugins": {
    "BluetoothLe": {
      "displayStrings": {
        "scanning": "Scanning for BLE devices...",
        "cancel": "Cancel",
        "availableDevices": "Available devices",
        "noDeviceFound": "No device found"
      }
    }
  }
}
```

### Android Gradle Config

**File:** `webclient/android/build.gradle`

```gradle
dependencies {
    classpath 'com.android.tools.build:gradle:8.13.0'
    classpath 'com.google.gms:google-services:4.4.4'
}
```

**File:** `webclient/android/variables.gradle`

```gradle
ext {
    minSdkVersion = 24
    compileSdkVersion = 36
    targetSdkVersion = 36
    // ... other versions
}
```

### Vite Config

**File:** `webclient/vite.config.js`

```javascript
export default defineConfig(({ mode }) => ({
  plugins: [react(), mkcert()],
  // Conditional base path
  base: process.env.CAPACITOR ? '/' : '/esp32c3zero-timer/',
  // ... other config
}))
```

## Permissions

The app requires the following Android permissions:

**Manifest:** `webclient/android/app/src/main/AndroidManifest.xml`

```xml
<uses-permission android:name="android.permission.BLUETOOTH" />
<uses-permission android:name="android.permission.BLUETOOTH_ADMIN" />
<uses-permission android:name="android.permission.BLUETOOTH_SCAN" />
<uses-permission android:name="android.permission.BLUETOOTH_CONNECT" />
<uses-permission android:name="android.permission.ACCESS_FINE_LOCATION" />
```

**Note:** Location permission is required for BLE scanning on Android, even though the app doesn't use GPS.

## Troubleshooting

### Build Issues

**"Invalid source release: 21"**
- Solution: Install Java 21 and update Codemagic config to use `java: 21`

**"Capacitor config not found"**
- Solution: Ensure `capacitor.config.json` exists (not `.js`)
- Delete `capacitor.config.js` if it exists

**"Browser Not Supported" error in app**
- Solution: Ensure `App.jsx` checks for Capacitor platform before checking `navigator.bluetooth`
- Fixed in v1.4 with platform detection

### Runtime Issues

**"Cannot connect to device"**
- Grant Bluetooth and Location permissions
- Ensure device is advertising (within 10 seconds of boot/wake)
- Check device is in range

**"Data not loading"**
- Check that DataFormatter methods use `decode*` not `parse*`
- Verify BleClient.read() returns DataView
- Check browser console for errors

**"App crashes on startup"**
- Check Android version (requires 7.0+)
- Verify all permissions granted
- Check logcat for native errors

## Development Workflow

### Making Changes

1. Edit React components in `src/`
2. Test in web browser first: `npm run dev`
3. Build for Android: `CAPACITOR=true npm run build`
4. Sync to Android: `npx cap sync android`
5. Test in Android emulator or device

### Testing

**Web Testing:**
```bash
npm run dev
# Open https://localhost:3000
```

**Android Testing:**
```bash
CAPACITOR=true npm run build
npx cap sync android
npx cap run android
```

### Debugging

**Web:**
- Use browser DevTools
- Check console for BLE logs

**Android:**
- Use Android Studio logcat
- Enable USB debugging
- Use Chrome DevTools for WebView debugging (chrome://inspect)

## Differences from Web Version

### BLE Implementation

**Web (useBLE.js):**
- Uses `navigator.bluetooth` API
- Direct access to GATT characteristics
- Returns `DataView` objects directly

**Android (useBLECapacitor.js):**
- Uses `BleClient` from Capacitor plugin
- Requires full UUID format (128-bit)
- Returns `DataView` objects from plugin

### Platform Detection

**App.jsx:**
```javascript
import { Capacitor } from '@capacitor/core';

const isNative = Capacitor.isNativePlatform();
if (!isNative && !navigator.bluetooth) {
  // Show browser not supported error
}
```

### UUID Format

**Web:** Accepts short UUIDs (0x00FF)
**Android:** Requires full UUIDs (000000ff-0000-1000-8000-00805f9b34fb)

Conversion handled in `useBLECapacitor.js`:
```javascript
const toFullUUID = (shortUuid) => {
  if (typeof shortUuid === 'number') {
    const hex = shortUuid.toString(16).padStart(4, '0');
    return `0000${hex}-0000-1000-8000-00805f9b34fb`;
  }
  return shortUuid;
};
```

## Version History

### v1.4 - Android App Support
- Initial Android app release
- Capacitor integration
- Unified BLE architecture
- CI/CD pipeline with Codemagic
- Platform-aware browser support check

## Future Enhancements

- [ ] iOS app support (requires alternative to Web Bluetooth)
- [ ] App store distribution (Google Play)
- [ ] Push notifications for device events
- [ ] Background BLE scanning
- [ ] Widget support
- [ ] Wear OS companion app

## Resources

- [Capacitor Documentation](https://capacitorjs.com/)
- [Capacitor Bluetooth LE Plugin](https://github.com/capacitor-community/bluetooth-le)
- [Android BLE Guide](https://developer.android.com/guide/topics/connectivity/bluetooth-le)
- [Codemagic Documentation](https://docs.codemagic.io/)

## License

See main project LICENSE file.
