


# Implementation Plan: BLE Web Interface

## Overview

This implementation plan prioritizes testing raw BLE connectivity first. After project setup, we create a minimal test UI that connects to the ESP32 and reads/writes raw bytes. Once BLE is verified working, we implement the DataFormatter to properly decode/encode data, then build the full application.

## Tasks

- [x] 1. Set up React project structure and dependencies
  - Create Vite-based React project in `webclient/` directory
  - Install dependencies: React 18, Vite
  - Configure Vite for development and production builds
  - Set up basic project structure (src/, public/, components/, hooks/, utils/)
  - Create placeholder files for all modules
  - _Requirements: All (project foundation)_

- [x] 2. Create minimal raw BLE test (no data formatting)
  - [x] 2.1 Create basic useBLE hook with raw connection only
    - Implement connect() function to request device "ESP32C3_Timer"
    - Connect to GATT server and discover service 0x00FF
    - Discover all characteristics (0x2A6E, 0x2A2B, 0xFF02, 0xFF05, 0xFF06, 0xFF07)
    - Set up disconnection event listener
    - Return connection state and characteristic references
    - _Requirements: 1.2, 1.3, 1.5, 1.6_

  - [x] 2.2 Create RawBLETest component
    - Add connect/disconnect buttons
    - Display connection status
    - Add buttons to read each characteristic (display raw bytes as hex)
    - Add button to write relay ON (0xFF02 with 0x01)
    - Add button to write relay OFF (0xFF02 with 0x00)
    - Display raw DataView/ArrayBuffer values for debugging
    - Log all BLE operations to console with timestamps
    - _Requirements: 1.1, 1.2, 1.3, 1.6, 3.2, 3.3_

  - [x] 2.3 Create minimal test App
    - Render RawBLETest component
    - Add basic styling for readability
    - Check for Web Bluetooth API support
    - _Requirements: 1.1, 1.2, 1.3, 10.4_

  - [x] 2.4 Test raw BLE connectivity with ESP32 device
    - Run development server (npm run dev)
    - Connect to ESP32C3_Timer device
    - Read temperature characteristic (0x2A6E) - verify you get 2 bytes
    - Read time characteristic (0x2A2B) - verify you get 10 bytes
    - Read relay state (0xFF02) - verify you get 1 byte
    - Write relay ON (0xFF02 with 0x01) - verify relay clicks and turns on
    - Write relay OFF (0xFF02 with 0x00) - verify relay clicks and turns off
    - Test disconnection when device sleeps (after 60 seconds)
    - Test reconnection when device wakes up
    - _Requirements: 1.1, 1.2, 1.3, 1.4, 1.5, 1.6, 3.2, 3.3_

- [x] 3. Checkpoint - Raw BLE connection verified
  - Ensure you can connect, read raw bytes, and control the relay before proceeding.

- [x] 4. Implement DataFormatter utility module
  - [x] 4.1 Implement temperature encoding and decoding functions
    - Write `decodeTemperature(dataView)` function
    - Write `encodeTemperatureCalibration(temp)` function
    - Handle signed 16-bit little-endian format
    - _Requirements: 8.1, 8.7_

  - [ ]* 4.2 Write property test for temperature round-trip
    - **Property 4: Temperature Data Round-Trip Consistency**
    - **Validates: Requirements 8.1, 8.7**

  - [x] 4.3 Implement threshold encoding and decoding functions
    - Write `decodeTemperatureThresholds(dataView)` function
    - Write `encodeTemperatureThresholds(high, low)` function
    - Handle 4-byte format with two signed 16-bit little-endian integers
    - _Requirements: 8.4, 8.6_

  - [ ]* 4.4 Write property test for threshold round-trip
    - **Property 5: Threshold Data Round-Trip Consistency**
    - **Validates: Requirements 8.4, 8.6**

  - [x] 4.5 Implement current time decoding function
    - Write `decodeCurrentTime(dataView)` function
    - Parse 10-byte Bluetooth SIG Current Time format
    - Return object with year, month, day, hour, minute, second, dayOfWeek
    - _Requirements: 8.2_

  - [ ]* 4.6 Write property test for current time decoding
    - **Property 6: Current Time Decoding Correctness**
    - **Validates: Requirements 8.2**

  - [x] 4.7 Implement schedule parsing and encoding functions
    - Write `decodeSchedule(text)` function to parse CRLF-separated schedule
    - Write `encodeSchedule(day, scheduleString)` function to create 25-byte array
    - _Requirements: 8.3, 8.5_

  - [ ]* 4.8 Write property test for schedule parsing
    - **Property 7: Schedule Parsing Correctness**
    - **Validates: Requirements 8.3**

  - [ ]* 4.9 Write property test for schedule encoding
    - **Property 8: Schedule Encoding Format**
    - **Validates: Requirements 8.5**

  - [x] 4.10 Implement relay state encoding function
    - Write `encodeRelayState(state)` function
    - Return Uint8Array with 0x00 for OFF, 0x01 for ON
    - _Requirements: 3.2, 3.3_

  - [x] 4.11 Implement validation functions
    - Write `validateScheduleString(scheduleString)` function
    - Write `validateTemperatureThresholds(high, low)` function
    - Write `validateDay(day)` function
    - _Requirements: 6.3, 8.5_

  - [ ]* 4.12 Write property test for threshold validation
    - **Property 1: Temperature Threshold Validation**
    - **Validates: Requirements 6.3**

- [x] 5. Checkpoint - Ensure all DataFormatter tests pass
  - Ensure all tests pass, ask the user if questions arise.

- [x] 6. Enhance useBLE hook with formatted data
  - [x] 6.1 Update useBLE hook to use DataFormatter
    - Integrate DataFormatter into read operations
    - Update deviceData state with decoded values
    - Use DataFormatter for all write operations
    - _Requirements: 2.1, 2.2, 2.3, 2.4, 3.2, 3.3, 5.1, 6.1_

  - [x] 6.2 Implement notification setup and handling
    - Write function to enable notifications for a characteristic
    - Set up notification handlers for temperature (0x2A6E) and time (0x2A2B)
    - Decode notification data using DataFormatter
    - Update deviceData state when notifications arrive
    - _Requirements: 11.1, 11.2, 11.3, 11.4_

  - [x] 6.3 Implement operation queuing logic
    - Create queue state and queue management functions
    - Queue operations when disconnected
    - Limit queue size to 10 operations
    - _Requirements: 7.3_

  - [ ]* 6.4 Write property test for operation queuing
    - **Property 2: Operation Queuing During Disconnection**
    - **Validates: Requirements 7.3**

  - [x] 6.5 Implement automatic reconnection logic
    - Detect disconnection via gattserverdisconnected event
    - Implement exponential backoff (5s, 10s, 20s)
    - Retry up to 3 times
    - Set error state after 3 failures
    - _Requirements: 7.6_

  - [x] 6.6 Implement queued operation execution on reconnection
    - Use useEffect to execute queue when connection is re-established
    - Execute operations in FIFO order
    - Clear queue after execution
    - _Requirements: 7.4_

  - [ ]* 6.7 Write property test for queued operation execution
    - **Property 3: Queued Operation Execution on Reconnection**
    - **Validates: Requirements 7.4**

- [x] 7. Update test UI with formatted data
  - [x] 7.1 Update RawBLETest component to show formatted data
    - Display decoded temperature in °C
    - Display decoded time as readable date/time
    - Display relay state as ON/OFF
    - Display schedule as 7-day grid
    - Display thresholds as High/Low temperatures
    - Keep raw byte display for debugging
    - _Requirements: 2.1, 2.2, 2.3, 2.4_

  - [x] 7.2 Add more write operation tests
    - Add buttons to write schedule for a day
    - Add buttons to write temperature thresholds
    - Add button to write calibration value
    - Add button to reset calibration (-999)
    - _Requirements: 5.6, 6.4, 6.7, 6.8_

  - [x] 7.3 Test all BLE operations with formatted data
    - Verify temperature displays correctly in °C
    - Verify time displays correctly
    - Verify schedule read/write works
    - Verify threshold read/write works
    - Verify calibration works
    - Test notifications update UI automatically
    - _Requirements: 2.1, 2.2, 2.3, 2.4, 5.1, 5.6, 6.1, 6.4, 6.7, 11.1, 11.2_

- [x] 8. Checkpoint - Full BLE interface verified and working
  - Ensure all BLE operations work correctly with proper data formatting before building production UI.

- [x] 9. Implement production UI components
  - [x] 9.1 Create Header component
    - Display app title
    - Show hamburger menu button
    - Display connection status indicator
    - Show connect button when disconnected
    - _Requirements: 1.1, 4.1, 4.2_

  - [x] 9.2 Create Sidebar component
    - Display navigation menu items (Dashboard, Timer Programming, Temperature Settings)
    - Handle menu item clicks
    - Toggle visibility based on isOpen prop
    - _Requirements: 4.2, 4.3, 4.4, 4.5, 4.6_

  - [x] 9.3 Create Notification component
    - Display success, error, info, and loading notifications
    - Auto-dismiss success notifications after 3 seconds
    - Support persistent notifications (loading, errors)
    - _Requirements: 10.1, 10.2, 10.3_

  - [ ]* 9.4 Write property test for loading indicator display
    - **Property 9: Loading Indicator Display**
    - **Validates: Requirements 10.1**

  - [ ]* 9.5 Write property test for error message display
    - **Property 10: Error Message Display**
    - **Validates: Requirements 10.3**

  - [x] 9.6 Create TemperatureDisplay component
    - Display temperature value with unit (°C)
    - Handle null/undefined temperature (show "--")
    - Style for prominence on dashboard
    - _Requirements: 2.1_

  - [x] 9.7 Create RelayGauge component
    - Display relay state visually (gauge or toggle-style indicator)
    - Show ON/OFF state clearly
    - Provide toggle switch for manual control
    - Disable toggle when not connected
    - Call onToggle callback when user toggles
    - _Requirements: 2.4, 3.1, 3.4_

- [x] 10. Implement Dashboard view component
  - [x] 10.1 Create Dashboard component structure
    - Use useBLE hook to access device data and operations
    - Display TemperatureDisplay component
    - Display RelayGauge component
    - Display threshold values (High/Low)
    - Display today's schedule
    - _Requirements: 2.1, 2.2, 2.3, 2.4_

  - [x] 10.2 Implement relay toggle handler
    - Call ble.writeRelayState when user toggles
    - Show loading notification during write
    - Show success notification on success
    - Show error notification on failure
    - Display manual override notification
    - _Requirements: 3.2, 3.3, 3.4, 3.5, 3.6_

  - [ ]* 10.3 Write unit tests for Dashboard component
    - Test relay toggle interaction
    - Test display of device data
    - Test disabled state when disconnected
    - _Requirements: 2.1, 2.2, 2.3, 2.4, 3.1, 3.2, 3.3_

- [x] 11. Implement TimerProgramming view component
  - [x] 11.1 Create ScheduleGrid component
    - Display 7x24 grid (days × hours)
    - Show H/L/O mode for each cell
    - Handle cell click to cycle through modes
    - Highlight current hour
    - _Requirements: 5.2, 5.3, 5.4_

  - [x] 11.2 Create TimerProgramming component
    - Use useBLE hook to access schedule data
    - Read schedule when component mounts
    - Manage local schedule state for editing
    - Track modified state
    - Display ScheduleGrid component
    - Show save button when modified
    - _Requirements: 5.1, 5.2, 5.5_

  - [x] 11.3 Implement schedule save handler
    - Write schedule for each modified day
    - Show loading notification during writes
    - Show success notification on success
    - Show error notification on failure
    - Retain previous schedule on failure
    - _Requirements: 5.6, 5.7, 5.8_

  - [ ]* 11.4 Write unit tests for TimerProgramming component
    - Test schedule grid rendering
    - Test cell click cycling
    - Test save operation
    - Test error handling
    - _Requirements: 5.1, 5.2, 5.3, 5.4, 5.6, 5.7, 5.8_

- [x] 12. Implement TemperatureSettings view component
  - [x] 12.1 Create TemperatureSettings component structure
    - Use useBLE hook to access threshold data and operations
    - Read thresholds when component mounts
    - Create input fields for High and Low temperatures
    - Create calibration input field
    - Create save and reset buttons
    - _Requirements: 6.1, 6.2, 6.6_

  - [x] 12.2 Implement threshold validation and save handler
    - Validate High > Low before saving
    - Show validation error if invalid
    - Call ble.writeTemperatureThresholds on save
    - Show loading notification during write
    - Show success notification on success
    - Show error notification on failure
    - _Requirements: 6.3, 6.4, 6.5_

  - [x] 12.3 Implement calibration handlers
    - Call ble.writeTemperatureCalibration with entered value
    - Call ble.writeTemperatureCalibration(-999) for reset
    - Show success/error notifications
    - Clear input after successful calibration
    - _Requirements: 6.7, 6.8_

  - [ ]* 12.4 Write unit tests for TemperatureSettings component
    - Test threshold validation
    - Test save operation
    - Test calibration operation
    - Test reset calibration operation
    - _Requirements: 6.1, 6.2, 6.3, 6.4, 6.7, 6.8_

- [x] 13. Implement production App component and routing
  - [x] 13.1 Replace test App with production App component
    - Initialize useBLE hook
    - Manage current view state
    - Manage sidebar open/close state
    - Render Header, Sidebar, and current view
    - Render Notification component for errors
    - _Requirements: 4.6, All views_

  - [x] 13.2 Implement view routing logic
    - Switch between Dashboard, TimerProgramming, and TemperatureSettings
    - Update view on navigation
    - Close sidebar after navigation
    - _Requirements: 4.6_

  - [x] 13.3 Implement connection flow
    - Call ble.connect when user clicks connect button
    - Show connecting status
    - Read initial data after connection (temp, time, relay, schedule, thresholds)
    - Enable notifications for temperature and time
    - Show connected status
    - _Requirements: 1.1, 1.2, 1.3, 1.5, 11.1, 11.2_

  - [x] 13.4 Implement disconnection handling
    - Show disconnected status when connection lost
    - Display estimated wake time (60-second cycle)
    - Disable controls that require connection
    - Show reconnection prompt after 3 failed attempts
    - _Requirements: 1.6, 7.2, 7.5, 7.6_

  - [x] 13.5 Implement browser compatibility check
    - Check for Web Bluetooth API support on mount
    - Show compatibility error if not supported
    - Recommend Chrome, Edge, or Opera
    - _Requirements: 10.4_

  - [x] 13.6 Implement permission error handling
    - Catch permission denied errors
    - Show instructions for enabling Bluetooth permissions
    - _Requirements: 10.5_

  - [ ]* 13.7 Write integration tests for App component
    - Test connection flow
    - Test disconnection handling
    - Test view navigation
    - Test error handling
    - _Requirements: 1.1, 1.2, 1.3, 1.6, 4.6, 10.4, 10.5_

- [x] 14. Implement responsive styling
  - [x] 14.1 Create global styles
    - Set up CSS variables for colors, spacing, typography
    - Define base styles for body, headings, buttons, inputs
    - _Requirements: 9.1, 9.2_

  - [x] 14.2 Style Header component
    - Position hamburger menu in top left
    - Style connection status indicator
    - Make responsive for mobile and desktop
    - _Requirements: 4.1, 9.1, 9.2, 9.6_

  - [x] 14.3 Style Sidebar component
    - Create slide-in animation
    - Style navigation items
    - Make responsive for mobile and desktop
    - _Requirements: 4.2, 9.1, 9.2_

  - [x] 14.4 Style Dashboard view
    - Create grid layout for components
    - Style temperature display prominently
    - Style relay gauge
    - Make responsive for mobile and desktop
    - _Requirements: 2.1, 2.4, 9.1, 9.2_

  - [x] 14.5 Style TimerProgramming view
    - Create 7x24 grid layout
    - Style schedule cells with color coding (H=red, L=yellow, O=gray)
    - Highlight current hour
    - Make responsive for mobile (scrollable grid)
    - _Requirements: 5.2, 5.3, 9.1, 9.2_

  - [x] 14.6 Style TemperatureSettings view
    - Create form layout
    - Style input fields and buttons
    - Make responsive for mobile and desktop
    - _Requirements: 6.2, 9.1, 9.2_

  - [x] 14.7 Style Notification component
    - Position notifications (top-right or bottom-center)
    - Style different notification types (success=green, error=red, info=blue, loading=gray)
    - Add slide-in/fade-out animations
    - _Requirements: 10.1, 10.2, 10.3_

- [ ] 15. Final checkpoint and testing
  - [ ] 15.1 Test complete connection flow
    - Connect to device
    - Verify all data loads correctly
    - Test notifications work
    - Test disconnection and reconnection
    - _Requirements: 1.1, 1.2, 1.3, 1.5, 1.6, 11.1, 11.2_

  - [ ] 15.2 Test all user interactions
    - Test relay toggle
    - Test schedule editing and saving
    - Test threshold editing and saving
    - Test calibration
    - _Requirements: 3.2, 3.3, 5.4, 5.6, 6.4, 6.7_

  - [ ] 15.3 Test error scenarios
    - Test connection timeout
    - Test write failures
    - Test validation errors
    - Test browser compatibility error
    - _Requirements: 1.4, 3.5, 5.8, 6.3, 10.4, 10.5_

  - [ ] 15.4 Test responsive design
    - Test on mobile viewport (< 768px)
    - Test on desktop viewport (> 768px)
    - Verify touch targets on mobile
    - _Requirements: 9.1, 9.2, 9.3, 9.4_

  - [ ] 15.5 Update README for webclient
    - Document how to run the development server
    - Document how to build for production
    - Document browser requirements
    - Document how to connect to the ESP32 device

- [ ] 16. Final checkpoint - Ensure all tests pass
  - Ensure all tests pass, ask the user if questions arise.

## Notes

- Tasks marked with `*` are optional and can be skipped for faster MVP
- Each task references specific requirements for traceability
- Checkpoints ensure incremental validation
- Property tests validate universal correctness properties using fast-check library
- Unit tests validate specific examples and edge cases
- All files should be created in the `webclient/` directory
- The Web Bluetooth API requires HTTPS in production (or localhost for development)
- **Task 2 creates a raw BLE test to verify connectivity immediately (no data formatting needed)**
- **Checkpoint at Task 3 ensures raw BLE works before implementing DataFormatter**
- **Task 7 updates the test UI with formatted data after DataFormatter is complete**
- **Checkpoint at Task 8 ensures full BLE interface works before building production UI**
