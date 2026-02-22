# BLE Testing Guide

This guide provides step-by-step instructions for testing the BLE connectivity with formatted data display for the ESP32C3_Timer device.

## Prerequisites

1. ESP32C3_Timer device powered on and within Bluetooth range
2. Browser with Web Bluetooth API support (Chrome, Edge, or Opera)
3. Development server running

## Starting the Development Server

```bash
cd webclient
npm run dev
```

The application will be available at `http://localhost:5173` (or the port shown in the terminal).

## Test Procedure

### 1. Initial Connection Test

1. Open the application in your browser
2. Verify the "Raw BLE Test Interface" page loads
3. Click the "Connect" button
4. Select "ESP32C3_Timer" from the device picker dialog
5. Wait for connection to establish
6. Verify the status shows "Connected"
7. Check the operation log for successful connection messages

**Expected Results:**
- Connection status changes from "Disconnected" → "Connecting..." → "Connected"
- Operation log shows:
  - `[BLE] Requesting device...`
  - `[BLE] Device found: ESP32C3_Timer`
  - `[BLE] Connecting to GATT server...`
  - `[BLE] Connected to GATT server`
  - `[BLE] Discovering service 0x00FF...`
  - `[BLE] Service discovered`
  - `[BLE] Discovering characteristics...`
  - `[BLE] All characteristics discovered`
  - `[BLE] Connection complete`

### 2. Read Temperature Characteristic (0x2A6E)

1. Click "Read Temperature (0x2A6E)" button
2. Verify the read results section displays the data
3. Check that you receive exactly 2 bytes
4. **Verify formatted data displays temperature in °C**

**Expected Results:**
- Read Results shows "Temperature" entry
- **Formatted display shows temperature like "21.48°C"**
- Raw data (in collapsible section) shows 2 bytes (e.g., "64 08")
- Bytes count shows: 2
- Operation log shows successful read with formatted value

### 3. Read Time Characteristic (0x2A2B)

1. Click "Read Time (0x2A2B)" button
2. Verify the read results section displays the data
3. Check that you receive exactly 10 bytes
4. **Verify formatted data displays readable date/time**

**Expected Results:**
- Read Results shows "Time" entry
- **Formatted display shows date/time like "2024-02-14 15:30:45 (Wed)"**
- Raw data (in collapsible section) shows 10 bytes
- Bytes count shows: 10
- Operation log shows successful read with formatted value

### 4. Read Relay State (0xFF02)

1. Click "Read Relay State (0xFF02)" button
2. Verify the read results section displays the data
3. Check that you receive exactly 1 byte
4. **Verify formatted data displays "ON" or "OFF"**

**Expected Results:**
- Read Results shows "Relay State" entry
- **Formatted display shows "ON" or "OFF"**
- Raw data (in collapsible section) shows 1 byte (either "00" or "01")
- Bytes count shows: 1
- Operation log shows successful read with formatted value

### 5. Write Relay ON Test

1. Click "Relay ON (0x01)" button
2. Listen for the relay click sound
3. Verify the relay physically turns on
4. Click "Read Relay State (0xFF02)" to confirm
5. Verify the relay state reads as "01"

**Expected Results:**
- Audible click from the relay
- Relay physically switches to ON position
- Operation log shows: `Relay ON written successfully`
- Reading relay state returns "01"

### 6. Write Relay OFF Test

1. Click "Relay OFF (0x00)" button
2. Listen for the relay click sound
3. Verify the relay physically turns off
4. Click "Read Relay State (0xFF02)" to confirm
5. Verify the relay state reads as "00"

**Expected Results:**
- Audible click from the relay
- Relay physically switches to OFF position
- Operation log shows: `Relay OFF written successfully`
- Reading relay state returns "00"

### 7. Read Schedule Characteristic (0xFF05)

1. Click "Read Schedule (0xFF05)" button
2. Verify the read results section displays the data
3. **Verify formatted data shows "7 days"**
4. **Verify the 7-day schedule grid appears below the read results**
5. **Check that the grid shows all 7 days (Mon-Sun) with 24 hours each**
6. **Verify color coding: H=red, L=yellow, O=gray**

**Expected Results:**
- Read Results shows "Schedule" entry
- **Formatted display shows "7 days"**
- **Schedule Grid displays with 7 rows (days) × 24 columns (hours)**
- **Each cell shows H, L, or O with appropriate color**
- Raw data (in collapsible section) shows the schedule data
- Operation log shows successful read

### 8. Read Thresholds Characteristic (0xFF06)

1. Click "Read Thresholds (0xFF06)" button
2. Verify the read results section displays the data
3. Check that you receive exactly 4 bytes
4. **Verify formatted data displays High and Low temperatures**

**Expected Results:**
- Read Results shows "Thresholds" entry
- **Formatted display shows "High: 22.00°C, Low: 18.00°C" (or similar values)**
- Raw data (in collapsible section) shows 4 bytes
- Bytes count shows: 4
- Operation log shows successful read with formatted values

### 9. Read Calibration Characteristic (0xFF07)

1. Click "Read Calibration (0xFF07)" button
2. Verify the read results section displays the data
3. Check that you receive exactly 2 bytes

**Expected Results:**
- Read Results shows "Calibration" entry
- Hex display shows 2 bytes
- Bytes count shows: 2
- Operation log shows successful read

### 10. Disconnection Test (Device Sleep)

1. Wait for approximately 60 seconds without any interaction
2. Observe the device entering deep sleep mode
3. Verify the connection status changes to "Disconnected"
4. Check the operation log for disconnection message

**Expected Results:**
- After ~60 seconds of inactivity, device disconnects
- Connection status changes to "Disconnected"
- Operation log shows: `[BLE] Device disconnected`
- All read/write buttons become disabled

### 11. Reconnection Test (Device Wake)

1. After disconnection, wait for the device to wake up (~60 seconds)
2. Click the "Connect" button
3. Select "ESP32C3_Timer" from the device picker
4. Verify successful reconnection
5. Test reading a characteristic to confirm connection works

**Expected Results:**
- Device appears in the picker after waking
- Connection re-establishes successfully
- All characteristics are accessible again
- Read/write operations work normally

### 12. Write Schedule Test

1. Ensure device is connected
2. In the "Write Schedule" section, set Day to 0 (Monday)
3. Enter a schedule string like "HHHHHHHHOOOOOOOOLLLLLLLL" (8H, 8O, 8L)
4. Click "Write Schedule" button
5. Wait for success message in operation log
6. Click "Read Schedule (0xFF05)" to verify
7. Check that Monday's row in the schedule grid matches what you wrote

**Expected Results:**
- Operation log shows: `Schedule written successfully for day 0`
- Reading schedule shows the updated Monday schedule
- Schedule grid displays the new pattern with correct colors

### 13. Write Temperature Thresholds Test

1. Ensure device is connected
2. In the "Write Temperature Thresholds" section, enter High: 25.0
3. Enter Low: 15.0
4. Click "Write Thresholds" button
5. Wait for success message in operation log
6. Click "Read Thresholds (0xFF06)" to verify
7. Check that formatted display shows the new values

**Expected Results:**
- Operation log shows: `Thresholds written successfully`
- Reading thresholds shows: "High: 25.00°C, Low: 15.00°C"
- Values match what was written

### 14. Write Calibration Test

1. Ensure device is connected and calibration characteristic is available
2. In the "Write Temperature Calibration" section, enter 21.5
3. Click "Write Calibration" button
4. Wait for success message in operation log
5. Read temperature to see if calibration affected the reading

**Expected Results:**
- Operation log shows: `Calibration written successfully`
- Temperature reading may change based on calibration

### 15. Reset Calibration Test

1. Ensure device is connected and calibration characteristic is available
2. Click "Reset Calibration (-999)" button
3. Wait for success message in operation log
4. Read temperature to verify calibration is reset

**Expected Results:**
- Operation log shows: `Calibration reset successfully`
- Temperature reading returns to uncalibrated value

### 16. Notification Test (if implemented in useBLE)

1. Ensure device is connected
2. Enable notifications for temperature and time (if useBLE supports it)
3. Wait for automatic updates
4. Verify that temperature and time update automatically without clicking read buttons

**Expected Results:**
- Temperature updates automatically every few seconds
- Time updates automatically
- Operation log shows notification received messages
- Formatted values update in real-time

## Troubleshooting

### Device Not Found
- Ensure the ESP32 device is powered on
- Check that the device is within Bluetooth range (typically 10 meters)
- Wait for the device to wake from sleep (60-second cycle)
- Try refreshing the browser page

### Connection Fails
- Check browser console for detailed error messages
- Verify you're using a supported browser (Chrome, Edge, Opera)
- Ensure Bluetooth is enabled on your computer
- Try disconnecting and reconnecting

### Read/Write Operations Fail
- Verify the connection is still active
- Check the operation log for specific error messages
- Try disconnecting and reconnecting
- Ensure the device hasn't entered sleep mode

### Web Bluetooth Not Supported
- Use Chrome, Edge, or Opera browser
- Ensure you're accessing via HTTPS or localhost
- Update your browser to the latest version

## Success Criteria

All tests pass if:
- ✅ Connection establishes successfully
- ✅ Temperature characteristic returns 2 bytes and displays in °C
- ✅ Time characteristic returns 10 bytes and displays as readable date/time
- ✅ Relay state characteristic returns 1 byte and displays as ON/OFF
- ✅ Schedule displays as 7-day grid with color coding
- ✅ Thresholds display as High/Low temperatures in °C
- ✅ Relay ON command physically activates the relay
- ✅ Relay OFF command physically deactivates the relay
- ✅ Schedule write updates the device and can be read back
- ✅ Threshold write updates the device and can be read back
- ✅ Calibration write works (if characteristic available)
- ✅ Calibration reset works (if characteristic available)
- ✅ All read operations display formatted data prominently
- ✅ Raw data is available in collapsible debug sections
- ✅ Disconnection is detected after device sleeps
- ✅ Reconnection works after device wakes
- ✅ All operations are logged with timestamps and formatted values
- ✅ Notifications update UI automatically (if implemented)

## Next Steps

Once BLE connectivity with formatted data is verified:
1. Proceed to build the production UI components (Tasks 9-14)
2. Implement the full Dashboard, Timer Programming, and Temperature Settings views
3. Add responsive styling for mobile and desktop
