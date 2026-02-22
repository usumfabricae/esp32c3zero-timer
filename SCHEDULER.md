# Temperature-Based Scheduler System

## Overview
The device includes a weekly scheduler that controls the bistable relay based on temperature readings and time-of-day schedules.

## How It Works

### Schedule Definition
- **7 schedules**: One for each day of the week (Monday to Sunday)
- **24 characters per schedule**: One character per hour (0-23)
- **Three modes per hour**:
  - `H` = High temperature mode
  - `L` = Low temperature mode
  - `O` = Off mode (no automatic control)

### Temperature Control Logic

**High Mode (`H`):**
- If temperature < HIGH_TEMP → Relay ON
- If temperature >= HIGH_TEMP → Relay OFF

**Low Mode (`L`):**
- If temperature < LOW_TEMP → Relay ON
- If temperature >= LOW_TEMP → Relay OFF

**Off Mode (`O`):**
- Relay remains in current state (no automatic control)

### Example Schedule
```
Schedule: "LLLLLLHHHHHHHHHHHHHHHHHH"
Hour:      0123456789012345678901234
          |--Low--|----High--------|

0-5:  Low mode (18°C threshold)
6-23: High mode (22°C threshold)
```

## BLE Characteristics

### 0xFF05 - Schedule Configuration
**Write Only** - Set schedule for a specific day

**Format:** 25 bytes
- Byte 0: Day of week (0=Monday, 6=Sunday)
- Bytes 1-24: Schedule string (24 characters: 'H', 'L', or 'O')

**Example:**
```
Write: [0, 'L','L','L','L','L','L','H','H','H','H','H','H','H','H','H','H','H','H','H','H','H','H','H','H']
Sets Monday schedule: Low 0-5, High 6-23
```

### 0xFF06 - Temperature Thresholds
**Read/Write** - Get/Set temperature thresholds

**Format:** 4 bytes (little-endian, signed 16-bit integers)
- Bytes 0-1: High temperature threshold (°C)
- Bytes 2-3: Low temperature threshold (°C)

**Example Write:**
```
[22, 0, 18, 0] = High: 22°C, Low: 18°C
```

**Example Read Response:**
```
[22, 0, 18, 0] = High: 22°C, Low: 18°C
```

## Default Configuration

### Default Schedules
All days: `"HHHHHHHHHHHHHHHHHHHHHHHH"` (High mode all day)

### Default Temperatures
- High Temperature: 22°C
- Low Temperature: 18°C

## Usage Examples

### Example 1: Weekday/Weekend Schedule
**Weekdays (Mon-Fri):** Low comfort at night, high during day
```
Day 0-4: "LLLLLLHHHHHHHHHHHHHHHHHH"
         0-5: Low (18°C), 6-23: High (22°C)
```

**Weekend (Sat-Sun):** High comfort all day
```
Day 5-6: "HHHHHHHHHHHHHHHHHHHHHHHH"
         All hours: High (22°C)
```

### Example 2: Energy Saving
**All days:** Off at night, low during day
```
Day 0-6: "OOOOOOOOLLLLLLLLLLLLOOOOO"
         0-7: Off, 8-20: Low (18°C), 21-23: Off
```

### Example 3: Precise Control
**Monday:** Different modes throughout the day
```
Day 0: "LLLLLLHHHHHHLLLLHHHHHHOO"
       0-5: Low (18°C)
       6-11: High (22°C)
       12-15: Low (18°C)
       16-21: High (22°C)
       22-23: Off
```

## BLE Configuration Workflow

### Using nRF Connect or Similar App

**1. Set Temperature Thresholds:**
```
Connect to device
Navigate to Service 0x00FF
Find Characteristic 0xFF06
Write: [0x16, 0x00, 0x12, 0x00]  // High=22°C, Low=18°C
```

**2. Set Monday Schedule:**
```
Find Characteristic 0xFF05
Write: [0x00, 'L','L','L','L','L','L','H','H','H','H','H','H','H','H','H','H','H','H','H','H','H','H','H','H']
```

**3. Set Tuesday Schedule:**
```
Write: [0x01, 'L','L','L','L','L','L','H','H','H','H','H','H','H','H','H','H','H','H','H','H','H','H','H','H']
```

**4. Repeat for remaining days (0x02-0x06)**

**5. Read Current Thresholds:**
```
Read Characteristic 0xFF06
Response: [0x16, 0x00, 0x12, 0x00]
```

## Operation

### During BLE Connection
- Device stays awake
- Checks temperature every second
- Compares with current hour's threshold
- Switches relay as needed (1-second pulse for bistable relay)

### During Deep Sleep
- All GPIOs at 0 (no power consumption)
- Bistable relay holds its mechanical state
- Wakes every 60 seconds
- Can check schedule and adjust relay if needed

### Time Synchronization
- NTP sync on first boot
- Re-sync every 6 hours
- Timezone: CET/CEST (Italy)
- Accurate hour-based scheduling

## Storage

### NVS (Non-Volatile Storage)
All configuration persists across reboots:
- 7 day schedules
- High temperature threshold
- Low temperature threshold
- Current relay state

## Logging

The device logs scheduler decisions:
```
I (12345) scheduler: Current mode: day=0, hour=8, mode='H'
I (12346) scheduler: Temp=20°C, Threshold=22°C (mode='H'), Relay=ON
I (12347) main: Scheduler: Turning relay ON
I (13347) gpio_manager: Switching bistable relay ON (GPIO6/7=HIGH, GPIO8/9=LOW for 1s)
I (14347) gpio_manager: Bistable relay switched, turning off all GPIOs
I (14348) gpio_manager: Bistable relay state: ON (saved to NVS)
```

## Tips

1. **Test schedules**: Start with simple schedules and verify behavior
2. **Temperature hysteresis**: The system switches immediately when crossing threshold - consider this when setting thresholds
3. **Off mode**: Use 'O' for hours when you want manual control only
4. **Bistable relay**: Remember it only needs a 1-second pulse to switch
5. **Deep sleep**: Relay state is maintained mechanically during sleep

## Troubleshooting

**Relay not switching:**
- Check current time is synchronized (read 0xFF01)
- Verify schedule is set correctly for current day/hour
- Check temperature reading (read 0xFF03)
- Ensure thresholds are set correctly (read 0xFF06)

**Wrong schedule active:**
- Verify day of week calculation (0=Monday, 6=Sunday)
- Check timezone is correct (CET/CEST)
- Confirm NTP sync was successful

**Temperature not accurate:**
- Calibration points in code: 20°C=1471mV, 30°C=1266mV
- Adjust if your sensor differs
