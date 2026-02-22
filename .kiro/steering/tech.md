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
- ESP Sleep (deep sleep management)
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

## Web Client

### Framework & Build System
- React 18.2 (functional components with hooks)
- Vite 4.x (build tool and dev server)
- JavaScript (ES6+ modules)

### Key Libraries
- react-dom for rendering
- vite-plugin-mkcert for HTTPS development

### Architecture
- Custom hooks pattern (useBLE.js for Web Bluetooth API)
- Component-based UI (Dashboard, TimerProgramming, TemperatureSettings)
- CSS modules for styling
- Web Bluetooth API for BLE communication

### Development Requirements
- Node.js v14+
- HTTPS required (Web Bluetooth API restriction)
- Chrome 56+, Edge 79+, or Opera 43+ browsers

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

# Run dev server (HTTPS on port 3000)
npm run dev

# Build for production
npm run build

# Preview production build
npm run preview
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
- Deep sleep duration and all timings configurable in config.h
