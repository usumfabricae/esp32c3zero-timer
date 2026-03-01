# ESP32-C3 Timer - Web Interface & Android App

A React-based application for connecting to and controlling the ESP32-C3 temperature-controlled relay timer via Bluetooth Low Energy (BLE). Available as both a web application (using Web Bluetooth API) and a native Android app (using Capacitor).

## Platforms

### Web Application
- **Browser Requirements:** Chrome 56+, Edge 79+, Opera 43+
- **Protocol:** Web Bluetooth API
- **Deployment:** HTTPS required (localhost allowed for development)
- **Live Demo:** https://usumfabricae.github.io/esp32c3zero-timer/

### Android Application
- **Requirements:** Android 7.0+ (API 24+)
- **Protocol:** Capacitor Bluetooth LE plugin
- **Build System:** Automated via Codemagic CI/CD
- **Distribution:** APK download from Codemagic artifacts
- **Package:** `net.superfede.BleWebInterface`

## Dual-Mode BLE Architecture

The app automatically detects the environment and uses the appropriate BLE implementation:

### Platform Detection

```
App.jsx
  └─> useBLEUnified.js (Auto-detection)
       ├─> useBLE.js (Web Bluetooth API - for web/PWA)
       └─> useBLECapacitor.js (Capacitor BLE - for Android app)
```

**Detection Logic:**
```javascript
import { Capacitor } from '@capacitor/core';

const isNative = Capacitor.isNativePlatform();
if (isNative) {
  // Use Capacitor BLE plugin
} else {
  // Use Web Bluetooth API
}
```

### Implementation Files

1. **useBLEUnified.js** - Smart wrapper with automatic platform detection
2. **useBLE.js** - Web Bluetooth API implementation (web/PWA)
3. **useBLECapacitor.js** - Capacitor Bluetooth LE implementation (Android app)
4. **dataFormatter.js** - Shared data conversion utilities

### Unified API

Both implementations provide identical API - no code changes needed in components:

```javascript
import { useBLEUnified } from './hooks/useBLEUnified.js';

function MyComponent() {
  const ble = useBLEUnified();
  
  // Same API works in both environments
  await ble.connect();
  await ble.readTemperature();
  await ble.writeRelayState(1, 60);
  await ble.disconnect();
}
```

### API Compatibility Matrix

| Method | Web Bluetooth | Capacitor BLE | Notes |
|--------|---------------|---------------|-------|
| connect() | ✅ | ✅ | Auto-reconnect on Capacitor |
| disconnect() | ✅ | ✅ | Full support |
| readTemperature() | ✅ | ✅ | Full support |
| readCurrentTime() | ✅ | ✅ | Full support |
| readRelayState() | ✅ | ✅ | Full support |
| writeRelayState() | ✅ | ✅ | Full support |
| readSchedule() | ✅ | ✅ | Chunked reading |
| writeSchedule() | ✅ | ✅ | Full support |
| readTemperatureThresholds() | ✅ | ✅ | Full support |
| writeTemperatureThresholds() | ✅ | ✅ | Full support |
| readBatteryLevel() | ✅ | ✅ | Full support |
| setupNotifications() | ✅ | ✅ | Not implemented on device |

### Platform Differences

#### UUID Format

**Web Bluetooth:**
- Accepts short UUIDs: `0x2A6E`
- Automatically expands to full format

**Capacitor BLE:**
- Requires full 128-bit UUIDs: `00002a6e-0000-1000-8000-00805f9b34fb`
- Conversion handled automatically by `toFullUUID()` helper

#### Connection Behavior

**Web Bluetooth:**
- Shows device picker on first connection
- Auto-reconnects to paired devices (Chrome 85+)
- Fast connection (1-2 seconds)

**Capacitor BLE:**
- Saves device ID to localStorage
- Smart scanning at XX:XX:57 (synchronized with device wake)
- Connection in 3-5 seconds
- Automatic retry on failure

#### Permissions

**Web Bluetooth:**
- Bluetooth permission requested by browser
- No additional setup needed

**Capacitor BLE:**
- Requires Android manifest permissions:
  ```xml
  <uses-permission android:name="android.permission.BLUETOOTH_SCAN" />
  <uses-permission android:name="android.permission.BLUETOOTH_CONNECT" />
  <uses-permission android:name="android.permission.ACCESS_FINE_LOCATION" />
  ```
- Automatically added by build script

### Deployment Options

#### Option 1: PWA (Web Bluetooth)
**Pros:**
- No app store needed
- Instant updates
- Works on desktop too
- Smaller download size
- Easy distribution

**Cons:**
- Requires Chrome/Edge browser
- iOS not supported
- Less "native" feel

#### Option 2: Android App (Capacitor)
**Pros:**
- Feels more native
- Can add to Play Store
- Better offline support
- More control over permissions
- Background capabilities

**Cons:**
- Larger download size
- Updates require new APK
- Android only
- More complex distribution

#### Option 3: Both!
Offer both options to users:
- PWA for quick access and desktop users
- Android app for users who prefer native apps
- Same codebase, same features

### Adding New BLE Features

When adding new characteristics:

1. **Update useBLE.js** (Web Bluetooth):
   ```javascript
   const readNewFeature = useCallback(async () => {
     const char = characteristics[CHAR_NEW_FEATURE];
     const value = await char.readValue();
     return DataFormatter.decodeNewFeature(value);
   }, [characteristics]);
   ```

2. **Update useBLECapacitor.js** (Capacitor):
   ```javascript
   const readNewFeature = useCallback(async () => {
     const data = await readCharacteristic(CHAR_NEW_FEATURE);
     return DataFormatter.decodeNewFeature(data);
   }, [readCharacteristic]);
   ```

3. **Update dataFormatter.js**:
   ```javascript
   decodeNewFeature(dataView) {
     // Decode logic
   }
   ```

4. **Test both platforms**

### Performance Comparison

**Web Bluetooth:**
- Connection: 1-2 seconds
- Memory: Low (direct browser API)
- CPU: Minimal overhead
- Battery: Efficient

**Capacitor BLE:**
- Connection: 2-3 seconds (with smart scanning)
- Memory: Moderate (native bridge)
- CPU: Slight overhead (bridge communication)
- Battery: Efficient (optimized scanning)

Both are performant enough for this use case.

### Development Workflow

1. **Edit React components** in `src/`
2. **Test in web browser** first:
   ```bash
   npm run dev
   # Open https://localhost:3000
   ```
3. **Build for Android**:
   ```bash
   CAPACITOR=true npm run build
   npx cap sync android
   ```
4. **Test on device**:
   ```bash
   npx cap run android
   ```

### Debugging

**Web Mode:**
- Use browser DevTools
- Check console for BLE logs
- Network tab for service worker

**Android Mode:**
- Use Android Studio logcat
- Chrome DevTools for WebView: `chrome://inspect`
- Filter logs:
  ```powershell
  adb logcat --pid=<PID> Capacitor/Console:* Device:* *:S
  ```



## Features

- **Real-time Monitoring:**
  - Temperature readings with visual display
  - Battery level with gauge and status indicator
  - Relay state with interactive gauge
  - Current time display
  
- **Device Control:**
  - Manual relay control with configurable duration (15 min to 4 hours)
  - Override end time display when manual control is active
  - Weekly schedule programming (7 days × 24 hours)
  - Temperature threshold configuration (High/Low modes)
  - Temperature sensor calibration
  
- **User Experience:**
  - Smart scanning synchronized with device wake times (XX:XX:57)
  - Fast connection (3-5 seconds vs 70 seconds previously)
  - Automatic reconnection with exponential backoff
  - Real-time notifications for all operations
  - Responsive design for mobile and desktop
  - Connection status indicators
  - Data reload functionality
  
- **Performance Optimizations (Android 15):**
  - Debounced BLE notifications (max 1/second using refs)
  - No infinite CSS animations (prevents continuous rendering)
  - Safe area insets for system bars (no content overlap)
  - 93% reduction in scan time (70s → 3-5s)
  - ~95% reduction in render frequency during idle
  - Estimated 70-80% reduction in battery consumption

## Browser Requirements

This application requires a browser with Web Bluetooth API support:
- **Chrome** 56+ (recommended)
- **Edge** 79+
- **Opera** 43+

**Important:** 
- Web Bluetooth requires **HTTPS** in production
- Localhost is allowed for development
- Not supported in Firefox or Safari

## Quick Start

### Prerequisites

**For Web Development:**
- Node.js v14 or higher
- npm or yarn package manager

**For Android Development:**
- All web prerequisites
- Android Studio
- Java 21 (for Gradle builds)

### Installation

```bash
cd webclient
npm install
```

### Development Server (Web)

```bash
npm run dev
```

The application will open at `https://localhost:3000` (HTTPS required for Web Bluetooth)

**Network Access:**
The dev server is configured to listen on all network interfaces (`0.0.0.0`), allowing access from other devices on your network:
- Local: `https://localhost:3000`
- Network: `https://YOUR_IP:3000` (e.g., `https://192.168.1.100:3000`)

**Mobile Testing:**
1. Find your computer's IP address (use `.\show-ip.ps1` or check network settings)
2. On your mobile device, navigate to `https://YOUR_IP:3000`
3. Accept the self-signed certificate warning
4. Connect to your ESP32 device via Bluetooth

**Note:** All devices must be on the same network, and you may need to allow port 3000 through your firewall.

### Building for Production

**Web Build:**
```bash
npm run build
```

**Android Build:**
```bash
# Build web assets for Android
CAPACITOR=true npm run build

# Sync to Android project
npx cap sync android

# Build APK (debug)
cd android
./gradlew assembleDebug

# Or open in Android Studio
npx cap open android
```

**Output Locations:**
- Web: `dist/` directory
- Android: `android/app/build/outputs/apk/debug/app-debug.apk`

### Preview Production Build

```bash
npm run preview
```

## Project Structure

```
webclient/
├── src/
│   ├── components/           # React UI components
│   │   ├── Dashboard.jsx     # Main dashboard view
│   │   ├── Header.jsx        # App header with connect button
│   │   ├── Sidebar.jsx       # Navigation sidebar
│   │   ├── TemperatureDisplay.jsx  # Temperature gauge
│   │   ├── RelayGauge.jsx    # Relay control gauge
│   │   ├── BatteryGauge.jsx  # Battery level gauge
│   │   ├── ScheduleGrid.jsx  # Weekly schedule editor
│   │   ├── TimerProgramming.jsx    # Schedule programming view
│   │   ├── TemperatureSettings.jsx # Threshold configuration
│   │   └── Notification.jsx  # Toast notifications
│   ├── hooks/
│   │   └── useBLE.js         # Web Bluetooth API hook
│   ├── utils/
│   │   └── dataFormatter.js  # BLE data conversion utilities
│   ├── styles/
│   │   └── global.css        # Global styles
│   ├── App.jsx               # Main app component
│   └── main.jsx              # React entry point
├── public/                   # Static assets
├── index.html                # HTML template
├── vite.config.js            # Vite configuration
├── package.json              # Dependencies and scripts
└── README.md                 # This file
```

## Connecting to ESP32 Device

### Connection Process

1. **Power On Device:** Ensure your ESP32 is powered and within Bluetooth range
2. **Click Connect:** Press the "Connect" button in the web interface
3. **Automatic Connection:**
   - **First Time:** Browser shows device picker → Select "ESP32C3_Timer"
   - **Subsequent Connections:** Automatically connects to previously paired device (no picker)
   - **Fallback:** If paired device unavailable, picker appears automatically
4. **Wait for Data:** Initial data will be read automatically after connection
5. **Disconnect:** Click "Disconnect" button to manually disconnect

### Connection Behavior

**Desktop (Chrome/Edge 85+):**
- First connection: Shows device picker for pairing
- Subsequent connections: Connects automatically without picker (if device advertising)
- If device unavailable: Shows picker as fallback

**Mobile (Chrome Android):**
- First connection: Shows device picker for pairing
- Subsequent connections: Attempts auto-connect, shows picker if device not available
- Smart scanning: Starts at XX:XX:57, device wakes at XX:XX:59
- Fast connection: Device found within 3-5 seconds

**Connection Button:**
- Shows "Connect" when disconnected
- Shows "Disconnect" when connected
- Button color changes to indicate state (blue = connect, red = disconnect)

**Synchronized Timing:**
- ESP32 wakes at XX:XX:59 each minute (active until XX:XX:05)
- Android app starts scanning at XX:XX:57 (3 seconds before device wake)
- Connection typically established within 3-5 seconds
- 93% reduction in scan time (from 70 seconds to 3-5 seconds)
- Automatic retry at next minute mark if device not found

**Performance Optimizations:**
- BLE notifications batched using refs (prevents rapid re-renders)
- State updates debounced to max 1/second
- Infinite CSS animations removed (connection pulse, schedule glow)
- Debug logging removed from render cycle
- Safe area insets for Android 15 system bars
- Result: Smooth UI, minimal battery drain, fast connections

### Connection Notes

- Device advertises from XX:XX:59 to XX:XX:05 each minute (6 seconds)
- Android app uses smart scanning starting at XX:XX:57
- Connection typically established within 3-5 seconds
- Previously paired devices reconnect automatically (Chrome 85+)
- Automatic reconnection attempts up to 3 times with exponential backoff
- Connection status shown in header and notifications
- Manual disconnect available at any time

### Troubleshooting Connection

**"No device found"**
- Device wakes at XX:XX:59 each minute - Android app automatically scans at XX:XX:57
- Wait for next minute mark if connection fails
- Ensure device is powered on
- Check Bluetooth is enabled on your device

**"Connection failed"**
- Device may have gone to sleep - try again
- Refresh the page and reconnect
- Check browser console for detailed error messages

**"Permission denied"**
- Grant Bluetooth permissions in browser settings
- On some systems, you may need to pair the device first

## Using the Interface

### Dashboard View

The main dashboard displays:
- **Temperature Display:** Current temperature with visual gauge
- **Relay Gauge:** Current relay state with manual control toggle
  - Duration selector (15 min, 30 min, 1 hour, 2 hours, 3 hours, 4 hours)
  - Override end time display when manual control is active
  - Toggle switch for ON/OFF control
- **Battery Gauge:** Battery level percentage and voltage
- **Temperature Thresholds:** Current High/Low temperature settings
- **Today's Schedule:** Visual grid showing today's hourly schedule
- **Reload Button:** Refresh all data from device

### Timer Programming View

Program weekly schedules:
- **7-Day Grid:** Visual editor for all days and hours
- **Three Modes:**
  - H (High): Relay ON if temp < High threshold
  - L (Low): Relay ON if temp < Low threshold
  - O (Off): No automatic control
- **Click to Edit:** Click any cell to cycle through modes
- **Save:** Changes are saved immediately to device

### Temperature Settings View

Configure temperature control:
- **High Threshold:** Temperature for High mode operation
- **Low Threshold:** Temperature for Low mode operation
- **Calibration:** Add calibration points for sensor accuracy
- **Reset Calibration:** Clear all calibration data

## BLE Characteristics Used

The web interface communicates with these BLE characteristics:

### Standard Characteristics (Bluetooth SIG)
- **0x2A6E** - Temperature (Environmental Sensing)
- **0x2A2B** - Current Time
- **0x2A19** - Battery Level (Battery Service)
- **0x2A29** - Manufacturer Name
- **0x2A24** - Model Number
- **0x2A26** - Firmware Revision

### Custom Characteristics
- **0xFF02** - Relay Control (Read/Write)
  - Write format: ASCII string "x,y" where x=state (0/1), y=duration in 15-min units
  - Read format: ASCII string "x,hh:mm" (with override) or "x" (no override)
  - Example write: "1,4" = Turn ON for 60 minutes
  - Example read: "1,14:30" = ON, override until 14:30
- **0xFF05** - Schedule Configuration (Read/Write)
- **0xFF06** - Temperature Thresholds (Read/Write)
- **0xFF07** - Temperature Calibration (Write)

For detailed BLE protocol documentation, see main project README.

## Data Format Utilities

The `dataFormatter.js` utility handles all BLE data conversions:

- **Temperature:** 16-bit signed integer (0.01°C resolution)
- **Time:** 10-byte Bluetooth SIG Current Time format
- **Battery:** 8-bit unsigned integer (percentage)
- **Relay State:** ASCII string format
  - Write: "x,y" where x=state (0/1), y=duration in 15-min units
  - Read: "x,hh:mm" (with override) or "x" (no override)
- **Schedule:** 25-byte format (day + 24 characters)
- **Thresholds:** 4-byte format (2x 16-bit temperatures)

## Notifications

The interface provides real-time feedback:
- **Success:** Green notifications for successful operations
- **Error:** Red notifications for failures
- **Info:** Blue notifications for status updates
- **Loading:** Gray notifications for ongoing operations

Notifications auto-dismiss after 3-5 seconds (configurable).

## Responsive Design

The interface adapts to different screen sizes:
- **Desktop:** Full dashboard with sidebar navigation
- **Tablet:** Responsive grid layout
- **Mobile:** Stacked layout with hamburger menu

## Development Tips

### Hot Module Replacement

Vite provides instant hot module replacement during development. Changes to components will update immediately without full page reload.

### Browser Console

Enable browser console to see:
- BLE connection logs
- Characteristic read/write operations
- Error messages and stack traces
- Performance metrics

### Testing

Test the interface with:
```bash
npm run dev
```

Then connect to your ESP32 device and verify:
1. Connection establishes successfully
2. All data reads correctly
3. Write operations work (relay, schedule, thresholds)
4. Notifications appear for all operations
5. Reconnection works after device sleep

## Production Deployment

### HTTPS Requirement

Web Bluetooth API requires HTTPS. For production:

1. **Generate SSL Certificate:**
   ```bash
   npm run generate-cert
   ```

2. **Configure Web Server:**
   - Use nginx, Apache, or similar
   - Enable HTTPS with valid certificate
   - Serve files from `dist/` directory

3. **Build and Deploy:**
   ```bash
   npm run build
   # Copy dist/ contents to web server
   ```

### Example nginx Configuration

```nginx
server {
    listen 443 ssl http2;
    server_name timer.example.com;
    
    ssl_certificate /path/to/cert.pem;
    ssl_certificate_key /path/to/key.pem;
    
    root /var/www/timer/dist;
    index index.html;
    
    location / {
        try_files $uri $uri/ /index.html;
    }
}
```

## Known Limitations

- **Browser Support:** Limited to Chromium-based browsers (web version)
- **Connection Range:** Bluetooth range typically 10-30 meters
- **Device Sleep:** Device sleeps between wake intervals for power saving (wakes at XX:XX:59)
- **Single Connection:** Device supports one BLE connection at a time
- **No Offline Mode:** Requires active BLE connection to device

## Future Enhancements

- [ ] Historical data charts
- [ ] Multiple device support
- [ ] Export/import schedules
- [ ] Dark mode theme
- [x] PWA support for mobile installation (see PWA_SETUP.md)
- [ ] Notification sound options

## PWA (Progressive Web App) Support

The web interface supports installation as a Progressive Web App on Android devices!

### Benefits
- Install as native-feeling app on home screen
- Standalone mode (no browser UI)
- Faster loading with service worker caching
- Works identically to web version
- Offline caching for app shell

### Requirements
- Android device with Chrome/Edge browser
- HTTPS connection (provided by GitHub Pages or localhost)
- App icons (192x192 and 512x512 PNG)

### Installation Steps

**For Users:**
1. Visit the app URL in Chrome/Edge on Android
2. Wait for install banner or tap menu (⋮) → "Install app"
3. Confirm installation
4. App icon appears on home screen

**For Developers:**
1. Generate app icons:
   ```bash
   cd webclient
   # Open generate-icons.html in browser and download icons
   # Or use: node create-pwa-icons.js (creates SVG, convert to PNG)
   ```

2. Place icons in `public/` folder:
   - `icon-192.png` (192x192 pixels)
   - `icon-512.png` (512x512 pixels)

3. Build and test:
   ```bash
   npm run build
   npm run preview
   # Visit https://localhost:4173/esp32c3zero-timer/
   ```

4. Verify PWA readiness:
   - Open Chrome DevTools → Application → Manifest
   - Check for errors
   - Verify icons are visible
   - Service worker should show "activated and running"

### PWA Configuration

**Manifest:** `public/manifest.json`
```json
{
  "name": "ESP32 Relay Timer",
  "short_name": "Relay Timer",
  "start_url": "/esp32c3zero-timer/",
  "display": "standalone",
  "theme_color": "#2196F3",
  "background_color": "#ffffff",
  "icons": [
    {
      "src": "/esp32c3zero-timer/icon-192.png",
      "sizes": "192x192",
      "type": "image/png"
    },
    {
      "src": "/esp32c3zero-timer/icon-512.png",
      "sizes": "512x512",
      "type": "image/png"
    }
  ]
}
```

**Service Worker:** `public/sw.js`
- Caches app shell for offline access
- Provides faster loading on repeat visits
- Update cache version to force refresh

### Customization

**Change App Name:**
Edit `public/manifest.json`:
```json
{
  "name": "Your Custom Name",
  "short_name": "Short Name"
}
```

**Change Theme Colors:**
```json
{
  "theme_color": "#your-color",
  "background_color": "#your-color"
}
```

### PWA Troubleshooting

**"Install" button doesn't appear:**
- Verify both icon files exist in `public/` folder
- Check icons are accessible at their URLs
- Open DevTools → Application → Manifest (check for errors)
- Ensure service worker is running
- Must be served over HTTPS
- Not in incognito mode
- Not already installed

**Icons show as broken:**
- Verify icon dimensions (exactly 192x192 and 512x512)
- Check file format is PNG
- Clear browser cache and rebuild

**Service worker not registering:**
- Check `sw.js` is in `public/` folder
- Verify no syntax errors in `sw.js`
- Clear browser cache
- Check browser console for errors

**App installs but won't open:**
- Verify `start_url` in manifest matches your URL structure
- Check `scope` is correct
- Test by visiting start_url directly

**To clear cached PWA:**
1. Uninstall the app from home screen
2. Clear browser cache
3. Update cache version in `sw.js`
4. Rebuild and reinstall

### Testing PWA

**Local Testing:**
```bash
npm run build
npm run preview
# Open https://localhost:4173/esp32c3zero-timer/
# Check DevTools → Application tab
```

**Android Testing:**
1. Deploy to GitHub Pages
2. Visit URL on Android Chrome/Edge
3. Install from browser menu
4. Test standalone mode

**Lighthouse Audit:**
- Chrome DevTools → Lighthouse
- Run "Progressive Web App" audit
- Fix any reported issues

### Limitations
- **iOS Not Supported:** Web Bluetooth API not available on iOS
- **Requires Connection:** BLE requires active device connection
- **Partial Offline:** App shell cached, but device data requires connection
- **Browser Specific:** Only works in Chrome/Edge (Web Bluetooth requirement)



## Troubleshooting

**Interface won't load:**
- Check Node.js and npm are installed
- Run `npm install` to install dependencies
- Check for port conflicts (default: 3000)

**Can't connect to device:**
- Verify device is powered and in range
- Check browser supports Web Bluetooth
- Ensure HTTPS is enabled (or using localhost)
- Try refreshing the page

**Data not updating:**
- Check BLE connection status in header
- Try clicking "Reload Data" button
- Reconnect to device
- Check browser console for errors

**Schedule not saving:**
- Verify BLE connection is active
- Check for error notifications
- Try reconnecting and saving again

## Support

For issues and questions:
- Check main project README for firmware documentation
- Review browser console for detailed error messages
- Ensure firmware and web interface versions are compatible

## License

See main project LICENSE file.

 
 


## Android App Development

### Building the Android App

#### Local Build

```bash
cd webclient

# 1. Build web assets for Android
CAPACITOR=true npm run build

# 2. Sync to Android project
npx cap sync android

# 3. Build APK
cd android
./gradlew assembleDebug

# Or open in Android Studio
npx cap open android
```

#### CI/CD Build (Codemagic)

The project includes automated builds via Codemagic:

**Configuration:** `codemagic.yaml`

**Triggers:**
- Push to `main` branch
- Pull requests
- Git tags

**Build Steps:**
1. Install npm dependencies
2. Check for existing Capacitor config
3. Add Android platform (if not exists)
4. Build web app with `CAPACITOR=true`
5. Sync Capacitor
6. Build Android debug APK

**Artifacts:**
- APK files available in Codemagic build artifacts
- Email notifications on build completion

### Android Configuration

#### Capacitor Config

**File:** `capacitor.config.json`

```json
{
  "appId": "net.superfede.BleWebInterface",
  "appName": "BLE Timer",
  "webDir": "dist",
  "server": {
    "androidScheme": "https"
  },
  "android": {
    "buildOptions": {
      "releaseType": "APK"
    }
  }
}
```

#### Gradle Configuration

**Build Tools:** `android/build.gradle`
```gradle
dependencies {
    classpath 'com.android.tools.build:gradle:8.13.0'
}
```

**SDK Versions:** `android/variables.gradle`
```gradle
ext {
    minSdkVersion = 24        // Android 7.0+
    compileSdkVersion = 36    // Android 15
    targetSdkVersion = 36
}
```

#### Vite Configuration

**File:** `vite.config.js`

The build uses conditional base paths:
- Web: `base: '/esp32c3zero-timer/'` (GitHub Pages)
- Android: `base: '/'` (local files)

Set via environment variable: `CAPACITOR=true npm run build`

### Android Permissions

Required permissions in `android/app/src/main/AndroidManifest.xml`:

```xml
<uses-permission android:name="android.permission.BLUETOOTH" />
<uses-permission android:name="android.permission.BLUETOOTH_ADMIN" />
<uses-permission android:name="android.permission.BLUETOOTH_SCAN" />
<uses-permission android:name="android.permission.BLUETOOTH_CONNECT" />
<uses-permission android:name="android.permission.ACCESS_FINE_LOCATION" />
```

**Note:** Location permission is required for BLE scanning on Android.

### Platform Differences

#### BLE Implementation

**Web (useBLE.js):**
- Uses `navigator.bluetooth` API
- Accepts short UUIDs (0x00FF)
- Returns DataView objects directly

**Android (useBLECapacitor.js):**
- Uses `BleClient` from Capacitor plugin
- Requires full 128-bit UUIDs
- UUID conversion handled automatically:
  ```javascript
  const toFullUUID = (shortUuid) => {
    if (typeof shortUuid === 'number') {
      const hex = shortUuid.toString(16).padStart(4, '0');
      return `0000${hex}-0000-1000-8000-00805f9b34fb`;
    }
    return shortUuid;
  };
  ```

#### Platform Detection

**App.jsx:**
```javascript
import { Capacitor } from '@capacitor/core';

const isNative = Capacitor.isNativePlatform();
if (!isNative && !navigator.bluetooth) {
  // Show browser not supported error
}
```

### Android Troubleshooting

#### Build Issues

**"Invalid source release: 21"**
- Install Java 21
- Update Codemagic config: `java: 21`

**"Capacitor config not found"**
- Ensure `capacitor.config.json` exists (not `.js`)
- Delete `capacitor.config.js` if present

**"Browser Not Supported" in app**
- Check platform detection in `App.jsx`
- Verify Capacitor import is correct

#### Runtime Issues

**"Cannot connect to device"**
- Grant Bluetooth and Location permissions
- Ensure device is advertising (XX:XX:59 to XX:XX:05)
- Check device is in range

**"Characteristic not found"**
- Clear Bluetooth cache: Settings → Apps → Clear Data
- Unpair device: Settings → Bluetooth → Forget device
- Reinstall app to clear cached GATT services

**"Data not loading"**
- Check DataFormatter uses correct methods
- Verify BleClient.read() returns DataView
- Check logcat for native errors

#### Debugging

**View Logs:**
```powershell
# All logs
adb logcat --pid=<PID>

# Filtered logs (cleaner)
adb logcat --pid=<PID> Capacitor/Console:* Device:* *:S

# Remove noise
adb logcat --pid=<PID> | Select-String -Pattern "setRequestedFrameRate|ViewPostIme" -NotMatch
```

**Clear App Data:**
```powershell
# Find package name
adb shell pm list packages | Select-String -Pattern "ble|superfede"

# Clear data
adb shell pm clear net.superfede.BleWebInterface

# Or uninstall
adb uninstall net.superfede.BleWebInterface
```

**WebView Debugging:**
- Enable USB debugging on Android
- Open Chrome: `chrome://inspect`
- Select your app's WebView
- Use DevTools to debug JavaScript

### Android Installation (Users)

1. Download APK from Codemagic artifacts
2. Enable "Install from unknown sources" in Android settings
3. Install the APK
4. Grant Bluetooth and Location permissions
5. Open app and connect to ESP32 device

### Development Workflow

1. Edit React components in `src/`
2. Test in web browser: `npm run dev`
3. Build for Android: `CAPACITOR=true npm run build`
4. Sync to Android: `npx cap sync android`
5. Test in emulator or device: `npx cap run android`

