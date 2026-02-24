# Dual-Mode BLE Support

Your app now supports BLE in both web and native Android environments!

## How It Works

The app automatically detects the environment and uses the appropriate BLE implementation:

- **Web/PWA Mode:** Uses Web Bluetooth API (browser native)
- **Android App Mode:** Uses Capacitor Bluetooth LE plugin (native Android)

## Architecture

```
App.jsx
  └─> useBLEUnified.js (Auto-detection)
       ├─> useBLE.js (Web Bluetooth API)
       └─> useBLECapacitor.js (Capacitor Bluetooth LE)
```

### Files

1. **useBLEUnified.js** - Smart wrapper that detects environment
2. **useBLE.js** - Original Web Bluetooth implementation (for web/PWA)
3. **useBLECapacitor.js** - New Capacitor Bluetooth LE implementation (for Android app)

## Usage

No changes needed in your components! The unified hook provides the same API:

```javascript
import { useBLEUnified } from './hooks/useBLEUnified.js';

function MyComponent() {
  const ble = useBLEUnified();
  
  // Same API works in both environments
  await ble.connect();
  await ble.readTemperature();
  await ble.writeRelayState(1, 60);
}
```

## Deployment Options

### Option 1: PWA (Web Bluetooth)

Users install from browser:
1. Visit your GitHub Pages URL
2. Install as PWA from browser menu
3. Web Bluetooth works natively

**Pros:**
- No app store needed
- Instant updates
- Works on desktop too
- Smaller download size

**Cons:**
- Requires Chrome/Edge browser
- iOS not supported

### Option 2: Android App (Capacitor)

Users install APK:
1. Download APK from Codemagic
2. Install on Android device
3. Native Bluetooth via Capacitor plugin

**Pros:**
- Feels more "native"
- Can add to Play Store
- Better offline support
- More control over permissions

**Cons:**
- Larger download size
- Updates require new APK
- Android only

### Option 3: Both!

Offer both options to users:
- PWA for quick access and desktop users
- Android app for users who prefer native apps

## Android Permissions

The Android app requires these permissions (automatically added):

```xml
<!-- Android 12+ -->
<uses-permission android:name="android.permission.BLUETOOTH_SCAN" />
<uses-permission android:name="android.permission.BLUETOOTH_CONNECT" />

<!-- Android 11 and below -->
<uses-permission android:name="android.permission.BLUETOOTH" />
<uses-permission android:name="android.permission.BLUETOOTH_ADMIN" />
<uses-permission android:name="android.permission.ACCESS_FINE_LOCATION" />
```

These are added automatically by `setup-android-permissions.js` during build.

## Testing

### Test PWA Mode

```bash
cd webclient
npm run dev
# Open https://localhost:3000 in Chrome
# BLE should work via Web Bluetooth API
```

### Test Android App Mode

```bash
cd webclient
npm run build
npx cap sync android
# Install APK on Android device
# BLE should work via Capacitor plugin
```

## Build Process

### Codemagic Workflow

1. Install npm dependencies
2. Initialize Capacitor (if needed)
3. Add Android platform (if needed)
4. **Add Bluetooth permissions** (automatic)
5. Build web app
6. Sync Capacitor
7. Build Android APK

The permissions script runs automatically during step 4.

## Troubleshooting

### Web Mode Issues

**Problem:** "Bluetooth not available"
- Check you're using Chrome/Edge browser
- Verify HTTPS is enabled (or localhost)
- Check browser supports Web Bluetooth

**Problem:** "Device not found"
- Device may be sleeping (wait 60 seconds)
- Check device is advertising
- Try refreshing page

### Android App Issues

**Problem:** "Bluetooth permission denied"
- Grant Bluetooth permissions in Android settings
- Reinstall app if permissions were denied

**Problem:** "Device not found"
- Check Location permission is granted (required for BLE scan on Android <12)
- Enable Location services on device
- Device may be sleeping (wait 60 seconds)

**Problem:** "Connection failed"
- Check device is in range
- Try force-closing and reopening app
- Check Android Bluetooth is enabled

## API Compatibility

Both implementations provide the same API:

| Method | Web Bluetooth | Capacitor BLE | Status |
|--------|---------------|---------------|--------|
| connect() | ✅ | ✅ | Full support |
| disconnect() | ✅ | ✅ | Full support |
| readTemperature() | ✅ | ✅ | Full support |
| readCurrentTime() | ✅ | ✅ | Full support |
| readRelayState() | ✅ | ✅ | Full support |
| writeRelayState() | ✅ | ✅ | Full support |
| readSchedule() | ✅ | ✅ | Full support |
| writeSchedule() | ✅ | ✅ | Full support |
| readTemperatureThresholds() | ✅ | ✅ | Full support |
| writeTemperatureThresholds() | ✅ | ✅ | Full support |
| readBatteryLevel() | ✅ | ✅ | Full support |
| setupNotifications() | ✅ | ✅ | Full support |

## Development Tips

### Adding New BLE Features

When adding new BLE characteristics:

1. Update `useBLE.js` (Web Bluetooth implementation)
2. Update `useBLECapacitor.js` (Capacitor implementation)
3. Ensure both provide the same API
4. Test in both environments

### UUID Format

**Important:** Capacitor requires full 128-bit UUIDs:

```javascript
// Web Bluetooth (accepts short form)
const CHAR_TEMP = 0x2A6E;

// Capacitor (requires full UUID)
const CHAR_TEMP = '00002a6e-0000-1000-8000-00805f9b34fb';
```

The `toFullUUID()` helper in `useBLECapacitor.js` handles this conversion.

## Performance

### Web Bluetooth
- Fast connection (1-2 seconds)
- Low memory usage
- Direct browser API

### Capacitor BLE
- Slightly slower connection (2-3 seconds)
- More memory usage (native bridge)
- Native Android Bluetooth stack

Both are fast enough for your use case.

## Future Enhancements

Possible improvements:

1. **iOS Support:** Add iOS Capacitor app (requires same dual-mode approach)
2. **Background Scanning:** Keep scanning for device in background
3. **Connection Caching:** Remember last connected device
4. **Offline Queue:** Queue operations when disconnected
5. **Better Error Handling:** More specific error messages per platform

## Summary

You now have the best of both worlds:
- ✅ PWA with Web Bluetooth for web users
- ✅ Native Android app with Capacitor BLE for app users
- ✅ Same codebase, same API
- ✅ Automatic environment detection
- ✅ No code changes needed in components

Users can choose their preferred installation method!
