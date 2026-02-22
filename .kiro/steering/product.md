---
inclusion: always
---

# Product Overview

ESP32-C3 temperature-controlled bistable relay timer with weekly scheduling capabilities.

## Core Functionality

- Temperature-based relay control with configurable thresholds (High: 22°C, Low: 18°C)
- Weekly scheduler with hourly granularity (7 days × 24 hours)
- Three operating modes per hour: High (H), Low (L), Off (O)
- Bistable relay with H-bridge control (1-second pulse switching)
- BLE interface for configuration and monitoring
- Web-based control interface using Web Bluetooth API
- NTP time synchronization with CET/CEST timezone (Italy)
- Power-efficient deep sleep operation (60-second wake intervals)

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
- Persistent storage (NVS) for schedules, thresholds, and state
- Manual override capability (1-hour duration, configurable)
- Smart temperature calibration with linear regression
- Automatic WiFi disconnect after NTP sync to save power
- LED status indicators (red=syncing, green=synced, blue=BLE connected, yellow=low battery)
- Centralized configuration in config.h file
- Real-time BLE notifications for battery, temperature, and time

## Web Client Interface

React-based single-page application for device control via Web Bluetooth API:

- Real-time temperature and battery monitoring
- Visual relay state indicator (gauge component)
- Battery level gauge with status indicator
- Interactive weekly schedule grid editor (7 days × 24 hours)
- Temperature threshold configuration (High/Low modes)
- Temperature sensor calibration interface
- Connection status and notifications
- Responsive design for mobile and desktop
- HTTPS required (Web Bluetooth API security requirement)
- Browser support: Chrome 56+, Edge 79+, Opera 43+
