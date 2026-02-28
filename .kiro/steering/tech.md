---
inclusion: always
---

# Technology Stack

## Embedded Firmware (ESP32-C3)

### Framework & Build System
- ESP-IDF v5.x (Espressif IoT Development Framework)
- CMake build system
- FreeRTOS for task management
- Component-based architecture

### Core Libraries
- ESP WiFi (station mode)
- ESP Bluetooth (BLE only, Bluedroid stack)
- NVS Flash (non-volatile storage)
- ESP ADC (temperature sensor)
- ESP Sleep (deep sleep management with precise timing)
- LWIP SNTP (NTP time sync)

### Language & Conventions
- C language (C11 standard)
- ESP-IDF logging macros (ESP_LOGI, ESP_LOGE, etc.)
- Component registration via CMakeLists.txt
- Header guards in all .h files
- Static const char *TAG for logging per module

### Configuration
- Centralized configuration in main/config.h
- All user-configurable parameters in single file with detailed comments
- WiFi credentials, device name, timings, thresholds, GPIO pins
- sdkconfig system for build-time ESP-IDF configuration
- Custom partition table (partitions.csv)
- BLE 4.2 features enabled for ESP32-C3 Zero compatibility

### Power Management
- **Synchronized timing:** Device wakes at XX:XX:59, sleeps at XX:XX:05
- **Active period:** 6 seconds (10% duty cycle)
- **Deep sleep:** 54 seconds (90% duty cycle)
- Dynamic sleep calculation based on actual wake time
- Immediate sleep entry if device wakes late
- Light sleep with BLE active during advertising period

## Web Client & Android App

### Framework & Build System
- React 18.2 (functional components with hooks)
- Vite 4.x (build tool and dev server)
- JavaScript (ES6+ modules)
- Capacitor 8.x (native Android wrapper)

### Key Libraries
- react-dom for rendering
- vite-plugin-mkcert for HTTPS development
- @capacitor/core for native platform detection
- @capacitor/android for Android platform
- @capacitor-community/bluetooth-le for Android BLE

### Architecture
- Custom hooks pattern (useBLE.js for Web Bluetooth API, useBLECapacitor.js for Capacitor)
- Unified BLE interface (useBLEUnified.js) with automatic platform detection
- Component-based UI (Dashboard, TimerProgramming, TemperatureSettings)
- CSS modules for scoped styling
- Web Bluetooth API for browser-based BLE communication
- Capacitor Bluetooth LE plugin for Android BLE communication
- **Smart scanning:** Starts at XX:XX:57 (3 seconds before device wake)
- **Debounced notifications:** BLE updates batched to max 1/second
- **Android 15 optimizations:** No infinite animations, safe area insets

### Development Requirements
- Node.js v14+
- HTTPS required for web (Web Bluetooth API restriction)
- Chrome 56+, Edge 79+, or Opera 43+ browsers for web
- Android Studio for Android development
- Java 21 for Android builds
- Network access on 0.0.0.0 for mobile testing

### Platform Support
- **Web:** Chrome, Edge, Opera on desktop and Android
- **Android:** Native app for Android 7.0+ (API 24+)
- **iOS:** Not supported (Web Bluetooth API not available on iOS)

## Common Commands

### Firmware Build & Deploy
```powershell
# Build firmware
.\build.ps1

# Flash and monitor (COM12)
.\deploy.ps1

# Restart device
.\restart.ps1

# View trace logs
.\trace.ps1
```

### Web Client Development
```bash
# Install dependencies
npm install

# Run dev server (HTTPS on port 3000, accessible on network)
npm run dev

# Access locally
https://localhost:3000

# Access from mobile device (same network)
https://YOUR_IP:3000

# Build for web production
npm run build

# Build for Android (sets CAPACITOR env var)
CAPACITOR=true npm run build

# Preview production build
npm run preview
```

### Android App Development
```bash
# Sync web build to Android
npx cap sync android

# Open in Android Studio
npx cap open android

# Build debug APK
cd android
./gradlew assembleDebug

# Build release APK
./gradlew assembleRelease
```

### ESP-IDF Direct Commands
```bash
# Build
idf.py build

# Flash to device
idf.py -p COM12 flash

# Monitor serial output
idf.py -p COM12 monitor

# Flash and monitor
idf.py -p COM12 flash monitor

# Clean build
idf.py fullclean

# Configure menuconfig
idf.py menuconfig
```

## Build Configuration Notes

- BLE 4.2 features must be enabled: `CONFIG_BT_BLE_42_FEATURES_SUPPORTED=y`
- Custom partition table required for WiFi + BT libraries
- All user configuration in main/config.h (WiFi, device name, timings, etc.)
- Temperature calibration stored in NVS
- **Timing configuration:**
  - `LIGHT_SLEEP_DURATION_SEC = 6` (fallback only)
  - `DEEP_SLEEP_DURATION_SEC = 54` (calculated dynamically)
  - `BLE_ADVERTISING_TIMEOUT_SEC = 6`

### Android Build Configuration
- Android Gradle Plugin 8.13.0 (requires Java 21)
- Minimum SDK: 24 (Android 7.0)
- Target SDK: 36 (Android 15)
- Capacitor 8.1.0
- Vite base path: `/` for Android, `/esp32c3zero-timer/` for web (conditional)
- Codemagic CI/CD: Automated builds on push to main branch

## Performance Optimizations

### ESP32 Firmware
- Precise timing: Wake at XX:XX:59, sleep at XX:XX:05
- Dynamic sleep calculation compensates for late wakes
- Immediate sleep entry if past target time
- 60% reduction in active time (6s vs 10s per minute)

### Android App
- Smart scanning: Starts at XX:XX:57 (synchronized with device)
- Scan duration: 3-5 seconds (vs 70 seconds previously)
- 93% reduction in scan time
- Debounced BLE notifications (max 1/second)
- No infinite CSS animations
- Safe area insets for Android 15 system bars
