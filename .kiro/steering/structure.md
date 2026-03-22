---
inclusion: always
---

# Project Structure

## Root Directory

```
/
├── main/                   # ESP32 firmware source code
├── webclient/              # React web interface
├── build/                  # Build artifacts (generated)
├── .kiro/                  # Kiro IDE configuration
├── .vscode/                # VS Code configuration
├── CMakeLists.txt          # Root CMake configuration
├── partitions.csv          # Custom partition table
├── sdkconfig*              # ESP-IDF configuration files
├── build.ps1               # Build script
├── deploy.ps1              # Flash and monitor script
├── restart.ps1             # Device restart script
├── trace.ps1               # Trace logging script
├── check-aws-iot-config.sh # AWS IoT Core configuration checker
├── esp32timer-policy-corrected.json # AWS IoT policy template
├── README.md               # Main documentation
├── README-IOTCORE.md       # AWS IoT Core integration guide
├── SCHEDULER.md            # Scheduler system documentation
├── BATTERY_CALIBRATION.md  # Battery calibration documentation
├── NEXT-STEPS-IOT.md       # IoT integration roadmap
├── QUICK-IOT-TEST.md       # Quick IoT testing guide
└── CODEMAGIC_SETUP.md      # CI/CD setup guide
```

## Firmware Structure (main/)

### Core Application
- `main.c` - Application entry point, boot sequence, deep sleep management
- `CMakeLists.txt` - Component registration and dependencies

### Functional Modules
- `config.h` - Centralized configuration file with all user-configurable parameters
- `config.h.example` - Example configuration template (without secrets)
- `ble_server.c/h` - BLE GATT server, characteristics, pairing
- `wifi_manager.c/h` - WiFi connection, NTP sync, time management
- `gpio_manager.c/h` - GPIO control, relay switching, temperature reading, battery monitoring
- `scheduler.c/h` - Weekly schedule logic, temperature-based control
- `connection_manager.c/h` - Dual-mode connection orchestration (IoT Core vs BLE)
- `iot_manager.c/h` - AWS IoT Core MQTT client, Device Shadow sync, delta processing
- `cert_store.c/h` - NVS-based X.509 certificate storage for IoT Core authentication
- `provision_certs.h` - Certificate provisioning helpers
- `led_status.c/h` - RGB LED status indicators
- `led_strip_encoder.c/h` - WS2812 LED driver (RMT-based)

### Module Responsibilities
- Each module has clear separation of concerns
- Header files expose public API only
- Static functions for internal implementation
- NVS storage handled within each module for its own data
- Logging uses module-specific TAG constants
- `connection_manager` decides IoT vs BLE mode each wake cycle based on reachability state
- `iot_manager` handles MQTT connect/publish/subscribe/disconnect within brief sync sessions
- `iot_manager` delta processing: supports partial thresholds (reads current config for missing field), resilient override handling (3-tier: `active` bool, `duration_minutes` inference, legacy `command` format), always-confirm pattern (echo delta to reported even on error), pre/post-delta diagnostic dumps
- `cert_store` manages X.509 certificates in NVS for IoT Core mTLS authentication
- `scheduler` exposes `scheduler_save_config()` for explicit NVS persistence (set functions only update in-memory)

## Web Client Structure (webclient/)

```
webclient/
├── src/
│   ├── components/         # React UI components
│   │   ├── Dashboard.jsx/css
│   │   ├── Header.jsx/css
│   │   ├── Sidebar.jsx/css
│   │   ├── ScheduleGrid.jsx/css
│   │   ├── TimerProgramming.jsx/css
│   │   ├── TemperatureSettings.jsx/css
│   │   ├── TemperatureDisplay.jsx/css
│   │   ├── RelayGauge.jsx/css
│   │   ├── BatteryGauge.jsx/css
│   │   ├── Notification.jsx/css
│   │   ├── Settings.jsx/css    # IoT Core connection settings
│   │   ├── PWAInstallPrompt.jsx/css
│   │   └── RawBLETest.jsx
│   ├── hooks/
│   │   ├── useBLE.js           # Web Bluetooth API hook
│   │   ├── useBLECapacitor.js  # Capacitor BLE hook
│   │   ├── useBLEUnified.js    # Platform detection & unified interface (IoT first, BLE fallback)
│   │   └── useIoT.js           # AWS IoT Core Device Shadow hook
│   ├── utils/
│   │   └── dataFormatter.js    # Data conversion utilities
│   ├── styles/
│   │   └── global.css          # Global styles
│   ├── App.jsx/css             # Main app component
│   └── main.jsx                # React entry point
├── android/                    # Capacitor Android project
│   ├── app/
│   │   ├── build.gradle        # Android build configuration
│   │   └── src/main/           # Android app source
│   ├── build.gradle            # Root Android build config
│   ├── gradle.properties       # Gradle properties
│   └── variables.gradle        # Android SDK versions
├── dist/                       # Production build output
├── node_modules/               # Dependencies
├── index.html                  # HTML template
├── vite.config.js              # Vite configuration (conditional base path)
├── capacitor.config.json       # Capacitor configuration
├── codemagic.yaml              # CI/CD pipeline configuration
├── package.json                # NPM dependencies and scripts
├── generate-cert.ps1           # HTTPS certificate generation
├── show-ip.ps1                 # Network IP display
├── README.md                   # Web client & Android app documentation
├── TESTING_GUIDE.md            # Testing procedures
└── MOBILE_TESTING.md           # Mobile testing guide
```

### Component Organization
- Each component has paired .jsx and .css files
- Components are self-contained and reusable
- Custom hooks for complex logic (useBLE, useBLECapacitor, useBLEUnified, useIoT)
- Utility functions for data formatting and conversion
- Platform-aware connection abstraction layer (IoT Core first, BLE fallback)
- Debounced BLE notifications (max 1/second) for performance
- Smart scanning synchronized with device wake times
- Optimistic state management via localDesiredRef in useIoT.js
- Dashboard Reload button calls `getDeviceShadow()` directly in IoT mode; read stubs return cached data
- Override writes include `ts` timestamp to prevent AWS Shadow from stripping fields in delta
- Override confirmation compares `reported.override` (not `reported.relay_state`)
- Pending commands indicator based solely on `localDesiredRef` keys
- Last sync time from nested shadow metadata path (`metadata.reported.timestamp.timestamp`)

## Architecture Patterns

### Firmware
- Component-based architecture (ESP-IDF style)
- Event-driven BLE communication
- Dual-mode connection manager (IoT Core vs BLE) with automatic fallback
- MQTT Device Shadow for cloud state synchronization
- Delta processing: subscribe to shadow delta, apply changes, confirm via reported state
- **Always-confirm pattern:** Echo delta fields back to reported even on processing errors (clears shadow queue)
- **Partial delta support:** Thresholds accept only `high` or only `low` (reads current config for missing field)
- **Resilient override handling:** 3-tier format support (active bool, duration inference, legacy command)
- **Pre/post-delta diagnostic dumps:** Temperature, battery, relay state, schedule logged before and after delta processing
- **NVS persistence:** `scheduler_save_config()` called explicitly after thresholds and schedule updates with error logging
- Persistent storage with NVS (schedules, thresholds, certificates, reachability state)
- Deep sleep for power efficiency (54s BLE mode, 300s IoT mode)

### Web Client
- React functional components with hooks
- Custom hook for BLE abstraction (useBLE for web, useBLECapacitor for Android)
- Custom hook for IoT Core (useIoT for AWS Device Shadow communication)
- Unified connection interface (useBLEUnified) tries IoT Core first (5s timeout), falls back to BLE
- Optimistic state management: localDesiredRef tracks pending writes until reported confirms
- Override writes include `ts` field; confirmation checks `reported.override` (ignoring `ts`)
- Dashboard Reload calls `getDeviceShadow()` directly in IoT mode; read stubs return cached data
- Pending commands based solely on `localDesiredRef` keys (not shadow desired vs reported)
- Manual refresh only — no auto-polling or BLE notifications that overwrite UI state
- Multi-app support: each app instance sees other apps' pending desired state
- Component composition for UI
- State management with useState/useEffect
- CSS modules for scoped styling
- Capacitor framework for native Android app

## Configuration Files

- `sdkconfig` - Generated ESP-IDF configuration (do not edit manually)
- `sdkconfig.defaults` - Default configuration values
- `sdkconfig.old` - Backup of previous configuration
- `partitions.csv` - Flash memory partition layout
- `vite.config.js` - Vite build and dev server settings
- `package.json` - NPM dependencies and scripts

## Documentation Files

- `README.md` - Main project documentation with BLE interface details
- `README-IOTCORE.md` - AWS IoT Core integration guide
- `SCHEDULER.md` - Detailed scheduler system documentation
- `BATTERY_CALIBRATION.md` - Battery calibration documentation
- `NEXT-STEPS-IOT.md` - IoT integration roadmap
- `QUICK-IOT-TEST.md` - Quick IoT testing guide
- `CODEMAGIC_SETUP.md` - CI/CD setup guide
- `webclient/README.md` - Web client setup and usage
- `webclient/TESTING_GUIDE.md` - Testing procedures
- `webclient/MOBILE_TESTING.md` - Mobile device testing
