# ESP32 Precise Timing Update

## Overview

Updated ESP32 firmware to use precise timing synchronized with the Android app's smart scanning feature.

## New Timing Behavior

### Wake/Sleep Cycle
- **Wake time:** XX:XX:59 (1 second before each minute mark)
- **Active period:** XX:XX:59 to XX:XX:05 (6 seconds)
  - BLE advertising
  - Light sleep with BLE active
  - Responsive to connections
- **Deep sleep:** XX:XX:05 to XX:XX:59 (54 seconds)
  - Full power down
  - BLE deinitialized
  - Minimal power consumption

### Example Timeline
```
08:35:59 - Wake from deep sleep, start BLE advertising
08:36:00 - Continue advertising (minute mark)
08:36:01 - Continue advertising
08:36:02 - Continue advertising
08:36:03 - Continue advertising (Android starts scanning at XX:XX:57)
08:36:04 - Continue advertising
08:36:05 - Enter deep sleep
...
08:36:59 - Wake from deep sleep, start BLE advertising
08:37:00 - Continue advertising (next minute mark)
```

## Synchronization with Android App

The Android app now uses smart scanning that starts at XX:XX:57 (3 seconds before minute mark):

1. **Android:** Starts scan at XX:XX:57
2. **ESP32:** Wakes at XX:XX:59 (2 seconds later)
3. **Connection:** Device found within 1-3 seconds
4. **Total scan time:** ~3-5 seconds (vs 70 seconds previously)

## Configuration Changes

### config.h
```c
#define LIGHT_SLEEP_DURATION_SEC 6   // Changed from 10
#define DEEP_SLEEP_DURATION_SEC 54   // Changed from 30
#define BLE_ADVERTISING_TIMEOUT_SEC 6 // Changed from 10
```

### main.c
- Added precise timing logic based on current second
- Device enters deep sleep at XX:XX:05 regardless of connection status
- Calculates exact sleep duration to wake at XX:XX:59
- Fallback to duration-based sleep if time is not available

## Benefits

1. **Predictable availability:** Device always available at same time each minute
2. **Faster connections:** Android knows exactly when to scan
3. **Power efficiency:** Shorter active period (6s vs 10s)
4. **Better synchronization:** Both devices work together for optimal timing

## Power Consumption

- **Active time:** 6 seconds per minute (10% duty cycle)
- **Deep sleep:** 54 seconds per minute (90% duty cycle)
- **Previous:** 10s active + 30s sleep = 25% duty cycle
- **Improvement:** 60% reduction in active time

## Testing Recommendations

1. **Verify wake timing:**
   - Monitor serial output for "Current time: HH:MM:SS"
   - Should show XX:XX:59 or XX:XX:00 on wake

2. **Verify sleep timing:**
   - Check "Will enter deep sleep in X seconds" message
   - Should enter sleep at XX:XX:05

3. **Test Android connection:**
   - App should find device within 3-5 seconds
   - Connection should be reliable at minute marks

4. **Test BLE connection persistence:**
   - Connect to device
   - Verify it stays awake while connected
   - Verify it recalculates sleep timing after disconnect

## Fallback Behavior

If time synchronization fails or time is not available:
- Uses `LIGHT_SLEEP_DURATION_SEC` (6 seconds) as fallback
- Uses `DEEP_SLEEP_DURATION_SEC` (54 seconds) for sleep duration
- Still maintains 60-second cycle, just not synchronized to minute marks

## Files Modified

1. `main/main.c` - Added precise timing logic
2. `main/config.h` - Updated timing constants and documentation
