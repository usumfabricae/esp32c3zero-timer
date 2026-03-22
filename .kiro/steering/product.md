---
inclusion: always
---

# ESP32-C3 Timer Control - Product Overview

ESP32-C3 temperature-controlled bistable relay timer with weekly scheduling capabilities, synchronized BLE connectivity, and AWS IoT Core cloud management.

## Core Functionality

- Temperature-based relay control with configurable thresholds (High: 22°C, Low: 18°C)
- Weekly scheduler with hourly granularity (7 days × 24 hours)
- Three operating modes per hour: High (H), Low (L), Off (O)
- Bistable relay with H-bridge control (1-second pulse switching)
- BLE interface for configuration and monitoring
- AWS IoT Core Device Shadow for cloud-based state management
- Web-based control interface using Web Bluetooth API or IoT Core
- NTP time synchronization with CET/CEST timezone (Italy)
- **Dual connectivity:** IoT Core (cloud) with automatic BLE fallback
- **Synchronized power-efficient operation:**
  - IoT mode: 5-minute wake cycles (WiFi + MQTT sync)
  - BLE mode: 1-minute wake cycles (XX:XX:59 to XX:XX:05)
  - Android app scans at XX:XX:57 for fast BLE connection (3-5 seconds)

## Hardware

- Platform: ESP32-C3 microcontroller
- Relay: Bistable (latching) type with parallel GPIO outputs
- Temperature sensor: ADC-based with multi-point calibration
- Battery monitor: ADC-based voltage monitoring with percentage calculation
- Status LED: WS2812 RGB LED for visual feedback
- GPIO configuration: GPIO2 (battery), GPIO3 (temp sensor), GPIO4/5 (relay B), GPIO6/7 (relay A), GPIO10 (LED)

## Key Features

- BLE GATT server with standard Bluetooth SIG characteristics
- Standard Battery Service (0x180F) for battery monitoring
- AWS IoT Core Device Shadow for remote state synchronization
- MQTT over TLS 1.2 with X.509 certificate authentication
- Persistent storage (NVS) for schedules, thresholds, state, and IoT certificates
- Manual override capability (1-hour duration, configurable)
- Smart temperature calibration with linear regression
- Smart battery calibration with linear regression
- Automatic WiFi disconnect after NTP/IoT sync to save power
- LED status indicators (red=syncing, green=synced, blue=BLE connected, yellow=low battery)
- Centralized configuration in config.h file
- Real-time BLE notifications for battery, temperature, and time (debounced to 1/second)
- Connection manager with automatic IoT/BLE mode switching and hourly IoT retry

## Web Client Interface

React-based application for device control available on multiple platforms:

- Real-time temperature and battery monitoring
- Visual relay state indicator (gauge component)
- Battery level gauge with status indicator
- Interactive weekly schedule grid editor (7 days × 24 hours)
- Temperature threshold configuration (High/Low modes)
- Temperature sensor calibration interface
- Battery sensor calibration interface
- IoT Core connection settings (thing name, credentials)
- Connection status with sync time and pending commands indicator
- Automatic reconnection for previously paired devices
- Connect/Disconnect button with visual state indication
- Responsive design for mobile and desktop
- Network-accessible dev server for mobile testing
- **Optimistic state management:**
  - Local desired state tracking survives shadow polls
  - Values only revert when device confirms via reported state
  - Multiple app instances see each other's pending changes
  - No time-based race conditions
- **Override writes include `ts` (timestamp)** to force AWS Shadow to always deliver all fields in delta
- **Pending commands indicator** based solely on `localDesiredRef` (not shadow desired vs reported comparison)
- **Dashboard Reload** calls `getDeviceShadow()` directly in IoT mode (read stubs return cached data)
- **Last sync time** extracted from nested shadow metadata (`metadata.reported.timestamp.timestamp`)
- **Optimized for Android 15:**
  - Debounced state updates (max 1/second)
  - No infinite CSS animations
  - Safe area insets for system bars
  - Smart scanning synchronized with device wake times

### Connectivity Modes

**IoT Core (Cloud):**
- AWS IoT Device Shadow for state synchronization
- Manual refresh only (no auto-polling) — user clicks Reload to fetch latest state
- Writes desired state, device applies on next sync
- Supports multiple simultaneous app instances
- Pending commands indicator shows unconfirmed changes

**BLE (Local):**
- Direct Bluetooth LE connection to device
- Real-time read/write of characteristics
- Automatic reconnection with exponential backoff
- Falls back from IoT Core on connection failure

### Platforms

**Web Application:**
- HTTPS required (Web Bluetooth API security requirement)
- Browser support: Chrome 56+, Edge 79+, Opera 43+
- Live demo: https://usumfabricae.github.io/esp32c3zero-timer/

**Android Application:**
- Android 7.0+ (API 24+)
- Native app via Capacitor framework
- Uses Capacitor Bluetooth LE plugin
- Automated builds via Codemagic CI/CD
- **Smart scanning:** Starts at XX:XX:57, finds device within 3-5 seconds
- **Power efficient:** 93% reduction in scan time vs previous implementation

## Power Efficiency

**ESP32 Device (IoT Mode):**
- Sync session: 5-30 seconds every 5 minutes
- WiFi + MQTT connect, publish state, process deltas, disconnect
- Hourly retry from BLE mode if IoT Core unreachable

**ESP32 Device (BLE Mode):**
- Active time: 6 seconds per minute (10% duty cycle)
- Deep sleep: 54 seconds per minute (90% duty cycle)
- 60% reduction in active time vs previous implementation

**Android App:**
- Scan time: 3-5 seconds (vs 70 seconds previously)
- 93% reduction in scan duration
- Predictable connection timing
