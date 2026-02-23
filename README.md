# ESP32-C3 Temperature-Controlled Bistable Relay Timer

## Overview
Temperature-controlled bistable relay system with weekly scheduling on ESP32-C3. Configure via BLE, automatic NTP time sync, and power-efficient deep sleep operation.

## Hardware Configuration

| Component | GPIO | Description |
|-----------|------|-------------|
| Relay Direction A | GPIO6 + GPIO7 | Parallel outputs (high current) |
| Relay Direction B | GPIO4 + GPIO5 | Parallel outputs (high current) |
| Temperature Sensor | GPIO3 | ADC1_CH3 input |
| Battery Voltage | GPIO2 | ADC1_CH2 input (voltage divider) |
| Status LED | GPIO10 | WS2812 RGB LED |

## Features

- **Bistable Relay**: H-bridge control with configurable pulse switching
- **Temperature Monitoring**: ADC-based sensor with multi-point calibration
- **Battery Monitoring**: Voltage monitoring with percentage calculation and low battery warning
- **Weekly Scheduler**: 7-day schedule with hourly temperature modes
- **BLE Control**: Full configuration via Bluetooth Low Energy with standard Battery Service
- **NTP Sync**: Automatic time sync with configurable timezone
- **Deep Sleep**: Configurable wake intervals for power efficiency
- **LED Indicators**: Visual status feedback including low battery warning
- **Web Interface**: React-based web app for device control via Web Bluetooth API
- **Centralized Configuration**: All settings in single config.h file

## Operation

### Boot Sequence
1. Initialize NVS and restore saved state
2. Connect to WiFi and sync time via NTP
3. Initialize GPIO manager and scheduler
4. Start BLE advertising for 10 seconds
5. If connected: Stay awake and monitor temperature/scheduler every 5 seconds
6. If no connection: Enter deep sleep (wake every 60s)

### Scheduler Logic

**Three Operating Modes:**
- **H (High)**: Relay ON if temp < 22°C, OFF if temp ≥ 22°C
- **L (Low)**: Relay ON if temp < 18°C, OFF if temp ≥ 18°C  
- **O (Off)**: No automatic control

**Default Schedule:**
- Hours 6-7: High mode (22°C threshold)
- All other hours: Off mode

**Monitoring Frequency:**
- Temperature and relay state checked every 5 seconds while BLE connected
- Scheduler evaluates conditions every 5 seconds
- Reduces log noise and power consumption

### Bistable Relay Operation

The relay is latching type - only needs a pulse to switch:

**Turn ON:** GPIO6/7=HIGH, GPIO4/5=LOW for 1s → All LOW  
**Turn OFF:** GPIO6/7=LOW, GPIO4/5=HIGH for 1s → All LOW

Relay mechanically holds state during deep sleep (all GPIOs LOW).

## BLE Interface

**Device Name:** ESP32C3_Timer  
**Service UUID:** 0x00FF  
**Advertising:** 10 seconds after boot/wake

### Standard Characteristics (Bluetooth SIG)

#### 0x2A6E - Temperature (Environmental Sensing)
- **Type:** Read, Notify
- **Format:** 2 bytes, signed 16-bit little-endian (0.01°C resolution)
- **Example:** `[0x64, 0x08]` = 2148 = 21.48°C
- **Conversion:** `temp_celsius = (byte[0] | (byte[1] << 8)) / 100.0`
- **Updates:** Real-time notifications when BLE connected
- **Compatible:** Generic BLE apps, smart home systems

#### 0x2A2B - Current Time
- **Type:** Read, Notify
- **Format:** 10 bytes (Bluetooth SIG Current Time format)
  - Bytes 0-1: Year (little-endian)
  - Byte 2: Month (1-12)
  - Byte 3: Day (1-31)
  - Byte 4: Hour (0-23)
  - Byte 5: Minute (0-59)
  - Byte 6: Second (0-59)
  - Byte 7: Day of week (1=Monday, 7=Sunday)
  - Byte 8: Fractions256
  - Byte 9: Adjust reason
- **Example:** `[0xE8, 0x07, 0x02, 0x08, 0x0E, 0x1E, 0x2D, 0x04, 0x00, 0x00]` = 2024-02-08 14:30:45 Thursday
- **Updates:** Real-time notifications when BLE connected
- **Compatible:** Time sync apps, smart home systems

#### 0x2A19 - Battery Level (Battery Service 0x180F)
- **Type:** Read, Notify
- **Format:** 1 byte (percentage 0-100)
- **Example:** `[0x4B]` = 75%
- **Calculation:** Based on voltage (4.2V = 100%, 3.0V = 0%)
- **Updates:** Real-time notifications when BLE connected
- **Compatible:** All BLE apps with battery monitoring
- **Note:** Voltage is read from GPIO2 with 2x multiplier (voltage divider compensation)

#### 0x2A29 - Manufacturer Name
- **Type:** Read
- **Format:** UTF-8 string
- **Value:** "ESP32"
- **Purpose:** Device identification

#### 0x2A24 - Model Number
- **Type:** Read
- **Format:** UTF-8 string
- **Value:** "ESP32C3-Timer-v1"
- **Purpose:** Device identification

#### 0x2A26 - Firmware Revision
- **Type:** Read
- **Format:** UTF-8 string
- **Value:** "1.0.0"
- **Purpose:** Version tracking

### Custom Characteristics

#### 0xFF02 - Relay Control
- **Type:** Read, Write
- **Format:** 1 byte (0=OFF, 1=ON)
- **Write:** `[0x01]` = ON, `[0x00]` = OFF
- **Read:** Returns current state
- **Manual Override:** Writing to this characteristic activates a 1-hour manual override
  - Scheduler is disabled for 1 hour after writing
  - After 1 hour, automatic scheduler control resumes
  - Useful for temporary manual control without changing schedule

#### 0xFF04 - Passkey Change
- **Type:** Write
- **Format:** 6 ASCII characters
- **Example:** `"123456"`
- **Status:** Reserved for future use (not enforced)

#### 0xFF05 - Schedule Configuration
- **Type:** Read, Write
- **Format (Write - Single Day Update):** 25 bytes
  - Byte 0: Day (0=Mon, 1=Tue, ..., 6=Sun)
  - Bytes 1-24: Schedule string ('H', 'L', or 'O')
- **Format (Write - Full Schedule Read Request):** 1 byte
  - Byte 0: `0xFF` (signals request to read full schedule)
- **Format (Read):** Chunked transfer (20 bytes per read)
  - Total: 180 bytes (7 days × 24 hours + 6 line breaks)
  - Format: 7 lines of 24 characters each, separated by `\r\n`
  - Client must:
    1. Write `0xFF` to request full schedule
    2. Read multiple times (20-byte chunks) until empty response
    3. Concatenate all chunks to get complete schedule
  - Line 1: Monday schedule
  - Line 2: Tuesday schedule
  - ...
  - Line 7: Sunday schedule

**Write Examples:**
```
Monday, High 6-7, Off rest:
[0x00, 'O','O','O','O','O','O','H','H','O','O','O','O','O','O','O','O','O','O','O','O','O','O','O','O']

Request full schedule read:
[0xFF]

Tuesday, Low 8-20, Off rest:
[0x01, 'O','O','O','O','O','O','O','O','L','L','L','L','L','L','L','L','L','L','L','L','O','O','O','O']

Sunday, High all day:
[0x06, 'H','H','H','H','H','H','H','H','H','H','H','H','H','H','H','H','H','H','H','H','H','H','H','H']
```

**Read Process:**
```
Step 1: Write [0xFF] to 0xFF05 (request full schedule)
Step 2: Read 0xFF05 → Get chunk 1 (20 bytes): "OOOOOOHHOOOOOOOOOOOO"
Step 3: Read 0xFF05 → Get chunk 2 (20 bytes): "OOOO\r\nOOOOOOOOOOOOOOOO"
Step 4: Read 0xFF05 → Get chunk 3 (20 bytes): "OOOOOOOO\r\nOOOOOOOOOO"
...continue until empty response...
Step 9: Read 0xFF05 → Get chunk 9 (4 bytes): "OOOO"
Step 10: Read 0xFF05 → Empty response (read complete)

Result after concatenation:
OOOOOOHHOOOOOOOOOOOOOOOO\r\n
OOOOOOOOOOOOOOOOOOOOOOOO\r\n
OOOOOOOOOOOOOOOOOOOOOOOO\r\n
OOOOOOOOOOOOOOOOOOOOOOOO\r\n
OOOOOOOOOOOOOOOOOOOOOOOO\r\n
OOOOOOOOOOOOOOOOOOOOOOOO\r\n
OOOOOOOOOOOOOOOOOOOOOOOO
```
(Monday has High mode at hours 6-7, all other days/hours are Off)

**Note:** Due to BLE MTU limitations (typically 23 bytes), large data transfers use chunked reading. The ESP32 maintains an internal offset and sends 20-byte chunks on each read request.

#### 0xFF06 - Temperature Thresholds
- **Type:** Read, Write
- **Format:** 4 bytes, 2x signed 16-bit little-endian
  - Bytes 0-1: High temperature (°C)
  - Bytes 2-3: Low temperature (°C)

**Examples:**
```
Set High=22°C, Low=18°C:
Write: [0x16, 0x00, 0x12, 0x00]

Set High=25°C, Low=20°C:
Write: [0x19, 0x00, 0x14, 0x00]

Read current:
Response: [0x16, 0x00, 0x12, 0x00] = High:22°C, Low:18°C
```

**Conversion:**
```
high_temp = byte[0] | (byte[1] << 8)
low_temp = byte[2] | (byte[3] << 8)
```

#### 0xFF07 - Temperature Calibration
- **Type:** Write
- **Format:** 2 bytes, signed 16-bit little-endian (temperature in °C)
- **Purpose:** Smart multi-point temperature calibration with linear regression
- **Special Value:** Write `-999` (0x19 0xFC) to reset calibration to factory defaults

**How it works:**
1. Stores up to 10 calibration points (temperature + voltage pairs)
2. Uses least-squares linear regression for best-fit curve
3. Automatically updates existing points if within 1°C
4. FIFO replacement when buffer is full (oldest point removed)
5. Saves all points to NVS for persistence across reboots
6. Can reset to factory defaults if calibration goes wrong

**Calibration Process:**
```
Step 1: Measure actual temperature: 15°C
        Write to 0xFF07: [0x0F, 0x00]
        → Point 1 added

Step 2: Wait for different temperature: 25°C
        Write to 0xFF07: [0x19, 0x00]
        → Point 2 added, regression calculated

Step 3: Add more points for better accuracy: 20°C
        Write to 0xFF07: [0x14, 0x00]
        → Point 3 added, regression recalculated

Step 4: Made a mistake? Reset everything:
        Write to 0xFF07: [0x19, 0xFC] (-999)
        → All calibration cleared, back to defaults
```

**Benefits:**
- **Accumulates data**: Each calibration improves accuracy
- **Smart updates**: Updates existing points if temperature is similar
- **Outlier resistant**: Linear regression smooths out bad readings
- **Reversible**: Can always reset to factory defaults
- **Persistent**: Survives reboots and power cycles
- **Automatic**: Calculates best-fit line from all points

**Example Usage:**
```
Day 1: Calibrate at 18°C → 1 point
Day 2: Calibrate at 22°C → 2 points (regression active)
Day 3: Calibrate at 10°C → 3 points (better curve)
Day 4: Calibrate at 28°C → 4 points (even better)
Mistake: Wrong reading → Write -999 to reset
```

## LED Status

| Color | Status |
|-------|--------|
| Red | Syncing time with NTP |
| Green | Time synchronized |
| Blue | BLE connected |
| Yellow | Low battery (< 30%) |
| Off | Deep sleep |

## Configuration with nRF Connect

1. **Connect:** Scan for "ESP32C3_Timer" and connect
2. **Read Device Info:**
   - Read 0x2A29 → "ESP32"
   - Read 0x2A24 → "ESP32C3-Timer-v1"
   - Read 0x2A26 → "1.0.0"
3. **Monitor Battery:**
   - Read 0x2A19 for battery level (0-100%)
   - Enable notifications for real-time updates
4. **Monitor Temperature:**
   - Read 0x2A6E for current temperature (standard format)
   - Enable notifications for real-time updates
5. **Monitor Time:**
   - Read 0x2A2B for current time (standard format)
   - Enable notifications for real-time updates
6. **View Current Schedule:**
   - Write `FF` to 0xFF05 (request full schedule)
   - Read 0xFF05 multiple times to get all chunks
   - Concatenate chunks until empty response
   - Parse result: 7 lines (Mon-Sun), 24 chars each, separated by `\r\n`
7. **Set Thresholds:** Write to 0xFF06: `16 00 12 00` (High=22°C, Low=18°C)
8. **Set Schedule:** Write to 0xFF05: `00` + 24 chars (for Monday)
9. **Control Relay:** Write to 0xFF02: `01` (ON) or `00` (OFF)

### Using Generic BLE Apps

The device supports standard Bluetooth SIG characteristics, making it compatible with generic BLE apps:

- **Battery monitoring:** Standard Battery Service (0x180F) with Battery Level (0x2A19)
- **Temperature monitoring:** Environmental Sensing Service (0x2A6E)
- **Time sync:** Current Time Service (0x2A2B)
- **Device info:** Properly identified in all BLE scanners
- **Real-time updates:** Notifications supported for battery, temperature, and time

**Benefits:**
- No custom app needed for basic monitoring
- Works with smart home systems
- Professional device identification
- Standard compliance
- Real-time data updates

## Persistent Storage (NVS)

Saved across reboots:
- Relay state
- 7 day schedules  
- Temperature thresholds
- Last NTP sync time
- BLE passkey

## Power Consumption

| Mode | Current |
|------|---------|
| BLE Connected | ~80mA |
| Deep Sleep | ~10µA |
| Relay Switching | ~200mA (1s) |

## Configuration

All device parameters are centralized in `main/config.h` for easy customization. This single file contains all configurable settings with detailed documentation.

### Quick Configuration

Edit `main/config.h` to customize:

```c
// Device identification
#define DEVICE_NAME "ESP32C3_Timer"

// WiFi settings
#define WIFI_SSID "YourNetwork"
#define WIFI_PASS "YourPassword"

// Power management
#define DEEP_SLEEP_DURATION_SEC 30        // Wake interval
#define BLE_ADVERTISING_TIMEOUT_SEC 10    // BLE window

// Time synchronization
#define NTP_RESYNC_INTERVAL_SEC (6 * 60 * 60)  // 6 hours
#define TIMEZONE_CONFIG "CET-1CEST,M3.5.0,M10.5.0/3"  // Italy

// Relay control
#define RELAY_HYSTERESIS_SEC (5 * 60)     // 5 min between switches
#define MANUAL_OVERRIDE_DURATION_SEC (60 * 60)  // 1 hour
#define RELAY_PULSE_DURATION_MS 500       // Pulse duration

// Temperature calibration
#define DEFAULT_TEMP_20C_MV 1471          // Voltage at 20°C
#define DEFAULT_TEMP_30C_MV 1266          // Voltage at 30°C
#define MAX_CALIBRATION_POINTS 10         // Max calibration points

// Battery monitoring
#define BATTERY_MAX_MV 4200               // 100% voltage
#define BATTERY_MIN_MV 3000               // 0% voltage
#define BATTERY_LOW_THRESHOLD_PERCENT 30  // Low battery warning

// BLE security
#define DEFAULT_PASSKEY 123456            // Pairing code

// GPIO pins
#define RELAY_GPIO_1A 6                   // Relay direction A
#define RELAY_GPIO_1B 7
#define RELAY_GPIO_2A 4                   // Relay direction B
#define RELAY_GPIO_2B 5
#define TEMP_SENSOR_GPIO 3                // Temperature sensor
#define BATTERY_VOLTAGE_GPIO 2            // Battery monitor
#define STATUS_LED_GPIO 10                // RGB LED

// LED brightness (0-255)
#define LED_BRIGHTNESS 10                 // Brightness level
```

### Configuration Categories

1. **Device Identification**: Device name for BLE and WiFi
2. **WiFi Configuration**: Network credentials and retry settings
3. **Power Management**: Sleep duration and BLE timeout
4. **Time Synchronization**: NTP interval and timezone
5. **Relay Control**: Hysteresis, override duration, pulse timing
6. **Temperature Sensor**: Calibration defaults and max points
7. **Battery Monitoring**: Voltage thresholds and warning level
8. **BLE Security**: Default pairing passkey
9. **GPIO Pin Assignments**: All hardware connections
10. **LED Brightness**: Status LED intensity

### Applying Configuration Changes

1. Edit `main/config.h`
2. Save the file
3. Rebuild firmware: `.\build.ps1`
4. Flash to device: `.\deploy.ps1`

### Configuration Examples

**Low Power Mode:**
```c
#define DEEP_SLEEP_DURATION_SEC 60
#define BLE_ADVERTISING_TIMEOUT_SEC 5
#define NTP_RESYNC_INTERVAL_SEC (24 * 60 * 60)
#define LED_BRIGHTNESS 5
```

**Responsive Mode:**
```c
#define DEEP_SLEEP_DURATION_SEC 15
#define BLE_ADVERTISING_TIMEOUT_SEC 30
#define RELAY_HYSTERESIS_SEC (2 * 60)
#define LED_BRIGHTNESS 30
```

**Different Timezone (US Eastern):**
```c
#define TIMEZONE_CONFIG "EST5EDT,M3.2.0/2,M11.1.0/2"
```

**Different Battery Type (3.7V LiPo):**
```c
#define BATTERY_MAX_MV 4200
#define BATTERY_MIN_MV 3000
#define BATTERY_LOW_THRESHOLD_PERCENT 25
```

For detailed configuration documentation, see comments in `main/config.h`.

## Troubleshooting

**Boot Issues:**
- Avoid GPIO8/9 (strapping pins) - now using GPIO4/5
- Check relay circuit doesn't pull GPIOs during boot

**Time Not Syncing:**
- Verify WiFi credentials in `main/config.h`
- Check internet connection
- Confirm NTP server reachable
- Read 0x2A2B to check current time

**Relay Not Switching:**
- Read 0x2A2B to verify time is synced
- Check schedule for current day/hour
- Read 0x2A6E to verify temperature
- Read 0xFF06 to check thresholds
- Verify `RELAY_PULSE_DURATION_MS` in config.h

**BLE Connection Fails:**
- Device advertises only for configured timeout (default 10 seconds)
- Reset device to restart advertising
- Ensure BLE enabled on phone
- Check `BLE_ADVERTISING_TIMEOUT_SEC` in config.h

**Battery Reading Incorrect:**
- Verify voltage divider circuit (should read 1/2 actual voltage)
- Adjust `BATTERY_MAX_MV` and `BATTERY_MIN_MV` in config.h
- Check GPIO2 connection

**LED Too Bright/Dim:**
- Adjust `LED_BRIGHTNESS` in config.h (0-255)

**Device Drains Battery Too Fast:**
- Increase `DEEP_SLEEP_DURATION_SEC` in config.h
- Decrease `LED_BRIGHTNESS` in config.h
- Increase `NTP_RESYNC_INTERVAL_SEC` in config.h

## Build Configuration

**Required Settings:**
- `CONFIG_BT_BLE_42_FEATURES_SUPPORTED=y` (fixes BLE issues on ESP32C3 Zero)
- Increased partition size for WiFi + BT libraries
- ESP-IDF v5.x

**Build Commands:**
```powershell
# Build firmware
.\build.ps1

# Flash and monitor
.\deploy.ps1

# Restart device
.\restart.ps1

# View trace logs
.\trace.ps1
```

**ESP-IDF Direct Commands:**
```bash
idf.py build
idf.py -p COM12 flash monitor
idf.py fullclean
idf.py menuconfig
```

## Web Interface

A React-based web application provides full device control via Web Bluetooth API.

**Live Demo:** https://usumfabricae.github.io/esp32c3zero-timer/

**Features:**
- Real-time temperature and battery monitoring
- Visual relay state indicator (gauge)
- Battery level gauge with status
- Interactive weekly schedule grid editor (7 days × 24 hours)
- Temperature threshold configuration
- Temperature sensor calibration interface
- Connection status and notifications
- Responsive design for mobile and desktop

**Requirements:**
- HTTPS connection (Web Bluetooth API security requirement)
- Chrome 56+, Edge 79+, or Opera 43+ browsers
- Node.js v14+ for development

**Quick Start:**
```bash
cd webclient
npm install
npm run dev
```

Access at `https://localhost:3000`

For detailed web client documentation, see `webclient/README.md`

## Project Structure

```
timer/
├── main/
│   ├── config.h            # Centralized configuration
│   ├── main.c              # Main application logic
│   ├── ble_server.c/h      # BLE GATT server
│   ├── wifi_manager.c/h    # WiFi and NTP
│   ├── gpio_manager.c/h    # GPIO and relay control
│   ├── scheduler.c/h       # Temperature scheduler
│   ├── led_status.c/h      # LED status indicators
│   └── CMakeLists.txt
├── webclient/              # React web interface
│   ├── src/
│   │   ├── components/     # UI components
│   │   ├── hooks/          # Custom hooks (useBLE)
│   │   └── utils/          # Data formatters
│   ├── package.json
│   └── README.md
├── README.md
├── SCHEDULER.md            # Detailed scheduler docs
├── BLE_SECURITY.md         # Security implementation
└── CMakeLists.txt
```

## Future Enhancements

- [ ] BLE passkey authentication enforcement
- [ ] WiFi provisioning via BLE
- [ ] Multiple temperature sensors
- [ ] MQTT integration
- [ ] OTA firmware updates
- [ ] Historical data logging
- [ ] Mobile app (iOS/Android)

## Recent Changes

### v1.2 - Configuration & Battery Monitoring
- **Centralized Configuration**: All settings moved to `main/config.h`
- **Battery Monitoring**: Added GPIO2 battery voltage monitoring with standard Battery Service (0x180F)
- **Battery LED Warning**: Yellow LED when battery < 30% (configurable)
- **Web Interface**: Added battery gauge to dashboard
- **Real-time Notifications**: Battery, temperature, and time updates via BLE notifications
- **Configuration Documentation**: Comprehensive config.h with detailed comments

### v1.1 - BLE Schedule Reading
- Implemented chunked transfer protocol for reading full 7-day schedule
- Client must write `0xFF` to request schedule, then read multiple 20-byte chunks
- Solves BLE MTU limitation (23 bytes) for 180-byte schedule data
- Maintains backward compatibility for single-day schedule writes

### v1.0 - Performance Optimization
- Reduced monitoring frequency from 1 second to 5 seconds
- Eliminated duplicate temperature logging
- Removed verbose relay state logging
- Cleaner logs with same functionality

## License

This project is licensed under the Apache-2.0 License.
