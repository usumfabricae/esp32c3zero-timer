# Requirements Document

## Introduction

A single-page web application that connects to the ESP32-C3 temperature-controlled relay timer via Bluetooth Low Energy (BLE). The application enables users to monitor temperature, view schedules, control the relay, and configure device settings from laptops or smartphones. The system must handle intermittent connectivity as the device wakes every 60 seconds to conserve power.

All web application files SHALL be located in the `webclient/` directory of the project.

## Glossary

- **Web_App**: The single-page web application running in a browser
- **ESP32_Device**: The ESP32-C3 temperature-controlled bistable relay timer
- **BLE_Connection**: Bluetooth Low Energy connection between Web_App and ESP32_Device
- **Relay_State**: Current state of the bistable relay (ON or OFF)
- **Temperature_Reading**: Current temperature value from the sensor
- **Schedule**: Weekly program defining temperature modes (H/L/O) for each hour
- **Temperature_Threshold**: High and Low temperature limits for relay control
- **Connection_Window**: 60-second interval when ESP32_Device is awake and available
- **Manual_Override**: User-initiated relay control that suspends automatic scheduling for 1 hour

## Requirements

### Requirement 1: BLE Device Connection

**User Story:** As a user, I want to connect to my ESP32 device via Bluetooth, so that I can monitor and control it from my laptop or smartphone.

#### Acceptance Criteria

1. WHEN a user opens the Web_App, THE Web_App SHALL display a connect button
2. WHEN a user clicks the connect button, THE Web_App SHALL scan for BLE devices with name "ESP32C3_Timer"
3. WHEN the ESP32_Device is found, THE Web_App SHALL establish a BLE_Connection
4. IF the ESP32_Device is not found within 10 seconds, THEN THE Web_App SHALL display a retry option with guidance about the 60-second wake cycle
5. WHEN the BLE_Connection is established, THE Web_App SHALL discover all GATT services and characteristics
6. WHEN the BLE_Connection is lost, THE Web_App SHALL display disconnected status and offer reconnection

### Requirement 2: Dashboard Display

**User Story:** As a user, I want to see current device status at a glance, so that I can quickly understand the system state.

#### Acceptance Criteria

1. WHEN the BLE_Connection is established, THE Web_App SHALL display the current Temperature_Reading
2. WHEN the BLE_Connection is established, THE Web_App SHALL display today's Schedule
3. WHEN the BLE_Connection is established, THE Web_App SHALL display the Temperature_Threshold values (High and Low)
4. WHEN the BLE_Connection is established, THE Web_App SHALL display the Relay_State using a visual gauge
5. WHEN Temperature_Reading updates, THE Web_App SHALL refresh the display within 2 seconds
6. WHEN Relay_State changes, THE Web_App SHALL update the gauge immediately

### Requirement 3: Manual Relay Control

**User Story:** As a user, I want to manually turn the relay on or off, so that I can override the automatic schedule when needed.

#### Acceptance Criteria

1. WHEN the BLE_Connection is established, THE Web_App SHALL display a toggle switch for relay control
2. WHEN a user toggles the switch to ON, THE Web_App SHALL write value 0x01 to characteristic 0xFF02
3. WHEN a user toggles the switch to OFF, THE Web_App SHALL write value 0x00 to characteristic 0xFF02
4. WHEN the write operation succeeds, THE Web_App SHALL update the toggle switch to reflect the new state
5. WHEN the write operation fails, THE Web_App SHALL display an error message and revert the toggle switch
6. WHEN Manual_Override is activated, THE Web_App SHALL display a notification that automatic scheduling is suspended for 1 hour

### Requirement 4: Navigation Menu

**User Story:** As a user, I want to access different configuration screens, so that I can manage device settings.

#### Acceptance Criteria

1. THE Web_App SHALL display a hamburger menu icon in the top left corner
2. WHEN a user clicks the hamburger menu, THE Web_App SHALL display navigation options
3. THE Web_App SHALL include "Timer Programming" option in the menu
4. THE Web_App SHALL include "Temperature Settings" option in the menu
5. THE Web_App SHALL include "Dashboard" option in the menu
6. WHEN a user selects a menu option, THE Web_App SHALL navigate to the corresponding screen

### Requirement 5: Timer Programming Interface

**User Story:** As a user, I want to view and update weekly schedules, so that I can program when the relay should operate.

#### Acceptance Criteria

1. WHEN a user navigates to Timer Programming, THE Web_App SHALL read characteristic 0xFF05 to retrieve the current Schedule
2. WHEN the Schedule is retrieved, THE Web_App SHALL display a 7-day by 24-hour grid
3. WHEN displaying the Schedule, THE Web_App SHALL show each hour as H (High), L (Low), or O (Off)
4. WHEN a user clicks on a time slot, THE Web_App SHALL allow selection between H, L, and O modes
5. WHEN a user modifies the Schedule, THE Web_App SHALL provide a save button
6. WHEN a user clicks save, THE Web_App SHALL write the updated Schedule to characteristic 0xFF05 for each modified day
7. WHEN the write operation succeeds, THE Web_App SHALL display a success confirmation
8. WHEN the write operation fails, THE Web_App SHALL display an error message and retain the previous Schedule

### Requirement 6: Temperature Settings Interface

**User Story:** As a user, I want to configure temperature thresholds and calibrate the sensor, so that the system operates according to my preferences.

#### Acceptance Criteria

1. WHEN a user navigates to Temperature Settings, THE Web_App SHALL read characteristic 0xFF06 to retrieve current Temperature_Threshold values
2. WHEN Temperature_Threshold values are retrieved, THE Web_App SHALL display High and Low temperature input fields
3. WHEN a user modifies temperature values, THE Web_App SHALL validate that High temperature is greater than Low temperature
4. WHEN a user saves temperature settings, THE Web_App SHALL write the values to characteristic 0xFF06 in the correct format (4 bytes, 2x signed 16-bit little-endian)
5. WHEN the write operation succeeds, THE Web_App SHALL display a success confirmation
6. THE Web_App SHALL provide a temperature calibration input field
7. WHEN a user enters a calibration temperature, THE Web_App SHALL write the value to characteristic 0xFF07 in the correct format (2 bytes, signed 16-bit little-endian)
8. THE Web_App SHALL provide a reset calibration button that writes -999 to characteristic 0xFF07

### Requirement 7: Intermittent Connectivity Handling

**User Story:** As a user, I want the app to handle the device's sleep cycle gracefully, so that I can use it despite the 60-second wake intervals.

#### Acceptance Criteria

1. WHEN the ESP32_Device enters deep sleep, THE Web_App SHALL detect the disconnection within 5 seconds
2. WHEN disconnection is detected, THE Web_App SHALL display a status message indicating the device is sleeping
3. WHEN the user attempts an operation while disconnected, THE Web_App SHALL queue the operation
4. WHEN the BLE_Connection is re-established, THE Web_App SHALL execute queued operations automatically
5. WHEN waiting for the device to wake, THE Web_App SHALL display estimated time until next Connection_Window
6. IF reconnection fails after 3 attempts, THEN THE Web_App SHALL prompt the user to manually reconnect

### Requirement 8: Data Format Handling

**User Story:** As a developer, I want the app to correctly encode and decode BLE characteristic values, so that communication with the device is reliable.

#### Acceptance Criteria

1. WHEN reading characteristic 0x2A6E (Temperature), THE Web_App SHALL decode 2 bytes as signed 16-bit little-endian and divide by 100 to get Celsius
2. WHEN reading characteristic 0x2A2B (Current Time), THE Web_App SHALL decode 10 bytes according to Bluetooth SIG Current Time format
3. WHEN reading characteristic 0xFF05 (Schedule), THE Web_App SHALL parse multi-line text with CRLF separators
4. WHEN reading characteristic 0xFF06 (Temperature Thresholds), THE Web_App SHALL decode 4 bytes as two signed 16-bit little-endian integers
5. WHEN writing to characteristic 0xFF05 (Schedule), THE Web_App SHALL format as 25 bytes: day index (0-6) followed by 24 characters
6. WHEN writing to characteristic 0xFF06 (Temperature Thresholds), THE Web_App SHALL encode as 4 bytes: two signed 16-bit little-endian integers
7. WHEN writing to characteristic 0xFF07 (Temperature Calibration), THE Web_App SHALL encode as 2 bytes: signed 16-bit little-endian integer

### Requirement 9: Responsive Design

**User Story:** As a user, I want the app to work well on both desktop and mobile devices, so that I can access it from any device.

#### Acceptance Criteria

1. THE Web_App SHALL adapt layout for screen widths below 768px (mobile)
2. THE Web_App SHALL adapt layout for screen widths above 768px (desktop)
3. WHEN displayed on mobile, THE Web_App SHALL use touch-friendly controls with minimum 44px touch targets
4. WHEN displayed on desktop, THE Web_App SHALL use mouse-friendly controls
5. THE Web_App SHALL maintain readability at all supported screen sizes
6. THE Web_App SHALL display the hamburger menu on all screen sizes

### Requirement 10: Error Handling and User Feedback

**User Story:** As a user, I want clear feedback about operations and errors, so that I understand what's happening with my device.

#### Acceptance Criteria

1. WHEN any BLE operation is in progress, THE Web_App SHALL display a loading indicator
2. WHEN a BLE operation succeeds, THE Web_App SHALL display a success message for 3 seconds
3. WHEN a BLE operation fails, THE Web_App SHALL display a descriptive error message
4. WHEN the browser does not support Web Bluetooth API, THE Web_App SHALL display a compatibility message with browser recommendations
5. WHEN BLE permissions are denied, THE Web_App SHALL display instructions for enabling Bluetooth permissions
6. WHEN connection is lost unexpectedly, THE Web_App SHALL display a notification and attempt automatic reconnection

### Requirement 11: Real-Time Data Updates

**User Story:** As a user, I want to see live updates of temperature and time, so that I have current information.

#### Acceptance Criteria

1. WHEN the BLE_Connection is established, THE Web_App SHALL enable notifications for characteristic 0x2A6E (Temperature)
2. WHEN the BLE_Connection is established, THE Web_App SHALL enable notifications for characteristic 0x2A2B (Current Time)
3. WHEN a notification is received for Temperature, THE Web_App SHALL update the display within 1 second
4. WHEN a notification is received for Current Time, THE Web_App SHALL update the display within 1 second
5. WHEN notifications stop arriving, THE Web_App SHALL detect potential disconnection within 10 seconds
