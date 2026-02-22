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
├── README.md               # Main documentation
├── BLE_SECURITY.md         # BLE security implementation guide
└── SCHEDULER.md            # Scheduler system documentation
```

## Firmware Structure (main/)

### Core Application
- `main.c` - Application entry point, boot sequence, deep sleep management
- `CMakeLists.txt` - Component registration and dependencies

### Functional Modules
- `config.h` - Centralized configuration file with all user-configurable parameters
- `ble_server.c/h` - BLE GATT server, characteristics, pairing
- `wifi_manager.c/h` - WiFi connection, NTP sync, time management
- `gpio_manager.c/h` - GPIO control, relay switching, temperature reading, battery monitoring
- `scheduler.c/h` - Weekly schedule logic, temperature-based control
- `led_status.c/h` - RGB LED status indicators
- `led_strip_encoder.c/h` - WS2812 LED driver (RMT-based)

### Module Responsibilities
- Each module has clear separation of concerns
- Header files expose public API only
- Static functions for internal implementation
- NVS storage handled within each module for its own data
- Logging uses module-specific TAG constants

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
│   │   └── RawBLETest.jsx
│   ├── hooks/
│   │   └── useBLE.js       # Web Bluetooth API hook
│   ├── utils/
│   │   └── dataFormatter.js # Data conversion utilities
│   ├── styles/
│   │   └── global.css      # Global styles
│   ├── App.jsx/css         # Main app component
│   └── main.jsx            # React entry point
├── dist/                   # Production build output
├── node_modules/           # Dependencies
├── index.html              # HTML template
├── vite.config.js          # Vite configuration
├── package.json            # NPM dependencies and scripts
├── generate-cert.ps1       # HTTPS certificate generation
├── show-ip.ps1             # Network IP display
├── README.md               # Web client documentation
├── TESTING_GUIDE.md        # Testing procedures
├── MOBILE_TESTING.md       # Mobile testing guide
└── CHECKPOINT_8_VERIFICATION.md
```

### Component Organization
- Each component has paired .jsx and .css files
- Components are self-contained and reusable
- Custom hooks for complex logic (useBLE)
- Utility functions for data formatting and conversion

## Architecture Patterns

### Firmware
- Component-based architecture (ESP-IDF style)
- Event-driven BLE communication
- State machine for connection management
- Persistent storage with NVS
- Deep sleep for power efficiency

### Web Client
- React functional components with hooks
- Custom hook for BLE abstraction (useBLE)
- Component composition for UI
- State management with useState/useEffect
- CSS modules for scoped styling

## Configuration Files

- `sdkconfig` - Generated ESP-IDF configuration (do not edit manually)
- `sdkconfig.defaults` - Default configuration values
- `sdkconfig.old` - Backup of previous configuration
- `partitions.csv` - Flash memory partition layout
- `vite.config.js` - Vite build and dev server settings
- `package.json` - NPM dependencies and scripts

## Documentation Files

- `README.md` - Main project documentation with BLE interface details
- `BLE_SECURITY.md` - Security implementation and pairing guide
- `SCHEDULER.md` - Detailed scheduler system documentation
- `webclient/README.md` - Web client setup and usage
- `webclient/TESTING_GUIDE.md` - Testing procedures
- `webclient/MOBILE_TESTING.md` - Mobile device testing
