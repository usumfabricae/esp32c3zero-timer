# Requirements Document

## Introduction

This document specifies requirements for integrating AWS IoT Core cloud connectivity into the ESP32-C3 temperature-controlled timer device. The primary objective is to minimize overall power consumption by optimizing the trade-off between connection frequency and connection power.

The integration adds cloud-based device management and control while maintaining backward compatibility with the existing BLE interface. The system uses a disconnected, periodic synchronization architecture where the device briefly connects to AWS IoT Core, synchronizes state via Device Shadow, receives pending commands, then disconnects and returns to deep sleep.

Power optimization strategy:
- IoT Mode (WiFi-based): WiFi consumes MORE power than BLE, therefore connect LESS frequently (every 5 minutes instead of every 1 minute). Deep sleep for almost 5 minutes provides significant power savings despite higher WiFi power draw during brief sync sessions.
- BLE Mode (fallback): BLE consumes LESS power than WiFi, therefore can afford to connect MORE frequently (every 1 minute). Shorter deep sleep cycles but lower power radio.

Key principle: WiFi should be active for the absolute minimum duration. The "connect, sync, disconnect, sleep" pattern prioritizes minimizing WiFi on-time to achieve lowest average power consumption.

## Glossary

- **ESP32_Device**: The ESP32-C3 microcontroller-based timer device with temperature sensor, relay control, and wireless connectivity
- **IoT_Core**: AWS IoT Core cloud service providing device connectivity, message routing, and device shadow functionality
- **Device_Shadow**: AWS IoT Core digital twin that stores and synchronizes device state asynchronously between the physical device and cloud applications
- **MQTT_Client**: The MQTT protocol client implementation on the ESP32_Device for brief synchronization sessions with IoT_Core
- **Synchronization_Session**: Brief connection period where device connects to IoT_Core, publishes state, receives commands, and disconnects (variable duration based on sync operations, maximum 30 seconds)
- **Web_Client**: The React-based web application that provides user interface for device control via Device_Shadow
- **BLE_Interface**: The existing Bluetooth Low Energy GATT server interface on the ESP32_Device
- **Connection_Manager**: Component responsible for selecting and managing the active connection method (IoT Core or BLE)
- **Certificate_Store**: Secure storage for AWS IoT Core device certificates and private keys in NVS flash
- **Status_Message**: MQTT message containing device telemetry (battery, temperature, relay state, time, schedule) published during sync
- **Delta_Update**: Device Shadow notification containing desired state changes that device must apply
- **Adaptive_Sleep**: Power management behavior that adjusts sleep duration and wake period based on IoT_Core reachability (IoT mode: 5-minute sleep cycle with variable wake duration for sync; BLE mode: 1-minute sleep cycle with fixed 6-second wake period)
- **NTP_Sync**: Network Time Protocol synchronization for device clock accuracy
- **Registration_Process**: The procedure for provisioning a new device with IoT_Core including certificate generation and policy attachment

## Requirements

### Requirement 1: AWS IoT Core Device Registration

**User Story:** As a device owner, I want to register my ESP32 device with AWS IoT Core, so that I can access and control it from anywhere via the cloud.

#### Acceptance Criteria

1. THE ESP32_Device SHALL store AWS IoT Core endpoint URL, device certificate, and private key in Certificate_Store
2. WHEN the ESP32_Device boots for the first time, THE ESP32_Device SHALL load IoT Core credentials from Certificate_Store
3. IF IoT Core credentials are missing, THEN THE ESP32_Device SHALL operate in BLE-only mode
4. THE Registration_Process SHALL generate unique device certificates using AWS IoT Core certificate authority
5. THE Registration_Process SHALL create device-specific MQTT topics following the pattern `esp32timer/{deviceId}/{message_type}`
6. THE Registration_Process SHALL attach an IoT policy granting publish/subscribe permissions to device topics and shadow access

### Requirement 2: MQTT Connection Establishment

**User Story:** As a device owner, I want my ESP32 device to automatically connect to AWS IoT Core when internet is available with minimal connection time, so that cloud-based control is seamless and power efficient.

#### Acceptance Criteria

1. WHEN the ESP32_Device completes NTP_Sync successfully, THE ESP32_Device SHALL attempt to establish brief MQTT connection to IoT_Core for synchronization
2. THE MQTT_Client SHALL use TLS 1.2 with mutual authentication using device certificates from Certificate_Store
3. WHEN MQTT connection succeeds, THE ESP32_Device SHALL publish a connection status message to topic `esp32timer/{deviceId}/status/connection`
4. IF MQTT connection fails after 10 seconds, THEN THE ESP32_Device SHALL immediately mark IoT_Core as unreachable and switch to BLE mode
5. THE MQTT_Client SHALL maintain connection only for duration of synchronization session (publish state and receive commands) to minimize WiFi on-time
6. WHEN MQTT connection is established, THE ESP32_Device SHALL subscribe to command topic `esp32timer/{deviceId}/commands/#` for duration of session
7. THE MQTT connection establishment SHALL be optimized to complete within 5 seconds to minimize WiFi power consumption

### Requirement 3: Device Shadow Synchronization

**User Story:** As a developer, I want device state synchronized via Device Shadow with minimal WiFi on-time, so that applications can read current state without direct device communication while maximizing power efficiency.

#### Acceptance Criteria

1. WHEN MQTT connection is established, THE ESP32_Device SHALL publish current device state (reported) to Device_Shadow
2. THE Device_Shadow SHALL contain battery level, temperature reading, relay state, current time, active schedule, and temperature thresholds
3. WHEN Device_Shadow has pending desired state changes, THE ESP32_Device SHALL receive delta notification during connection session
4. WHEN delta notification is received, THE ESP32_Device SHALL apply changes and publish shadow update confirmation before disconnecting
5. THE ESP32_Device SHALL complete entire synchronization session (publish reported state, receive delta, apply changes, confirm) within 30 seconds maximum
6. THE ESP32_Device SHALL stay awake only for actual sync duration (variable based on operations, typically 5-30 seconds) to minimize WiFi power consumption
7. THE ESP32_Device SHALL optimize sync session to minimize WiFi on-time by batching all operations before disconnecting
8. IF shadow synchronization fails, THEN THE ESP32_Device SHALL immediately mark IoT_Core as unreachable and switch to BLE mode

### Requirement 4: Status Message Publishing

**User Story:** As a cloud application, I want to receive device status updates when device wakes with minimal WiFi on-time, so that I can monitor device health and state while preserving battery life.

#### Acceptance Criteria

1. WHEN ESP32_Device wakes from deep sleep and establishes MQTT connection, THE ESP32_Device SHALL publish Status_Message to topic `esp32timer/{deviceId}/status/telemetry`
2. THE Status_Message SHALL include battery percentage, temperature in Celsius, relay state (H/L/O), Unix timestamp, and current schedule mode
3. WHEN ESP32_Device wakes in IoT mode, THE ESP32_Device SHALL publish Status_Message every 5 minutes (300 seconds) to minimize WiFi usage
4. WHEN ESP32_Device wakes in BLE mode with hourly IoT retry, THE ESP32_Device SHALL publish Status_Message every 60 minutes
5. THE Status_Message SHALL be published during synchronization session before disconnecting to minimize WiFi on-time
6. THE Status_Message SHALL use JSON format with schema version field for backward compatibility
7. THE Status_Message publishing SHALL complete within 2 seconds to minimize WiFi power consumption

### Requirement 5: Command Message Processing

**User Story:** As a user controlling the device remotely, I want to send commands via IoT Core with minimal device wake time, so that I can control the device without BLE proximity while preserving battery life.

#### Acceptance Criteria

1. WHEN Device_Shadow delta indicates override command, THE ESP32_Device SHALL activate manual override mode for specified duration
2. WHEN Device_Shadow delta indicates schedule update, THE ESP32_Device SHALL update weekly schedule in NVS and publish confirmation to reported section
3. WHEN Device_Shadow delta indicates threshold update, THE ESP32_Device SHALL update temperature thresholds in NVS and publish confirmation to reported section
4. WHEN Device_Shadow delta indicates calibration update, THE ESP32_Device SHALL update sensor calibration parameters in NVS and publish confirmation to reported section
5. THE ESP32_Device SHALL process all pending delta updates during synchronization session before disconnecting to minimize WiFi on-time
6. THE ESP32_Device SHALL batch all delta confirmations into single shadow update to minimize WiFi power consumption
7. IF delta update format is invalid, THEN THE ESP32_Device SHALL log error and continue with remaining delta updates without applying invalid change

### Requirement 6: Adaptive Sleep Behavior

**User Story:** As a device owner, I want the device to optimize power consumption by trading connection frequency for connection power efficiency, so that battery life is maximized.

#### Acceptance Criteria

1. WHEN IoT_Core is reachable, THE ESP32_Device SHALL wake every 300 seconds (5 minutes) for synchronization to minimize average power consumption (less frequent WiFi connections)
2. WHEN IoT_Core synchronization session completes successfully, THE ESP32_Device SHALL immediately disconnect MQTT and WiFi then enter deep sleep until next 5-minute wake cycle to minimize WiFi on-time
3. THE ESP32_Device SHALL stay awake in IoT mode only for duration needed to complete synchronization (variable duration, maximum 30 seconds) to minimize WiFi power consumption
4. WHEN IoT_Core is reachable, THE ESP32_Device SHALL NOT activate BLE_Interface during sleep cycles
5. WHEN IoT_Core is unreachable, THE ESP32_Device SHALL wake at XX:XX:59 each minute and activate BLE_Interface for exactly 6 seconds (XX:XX:59 to XX:XX:05) leveraging BLE's lower power consumption
6. WHEN IoT_Core is unreachable, THE ESP32_Device SHALL enter deep sleep for 54 seconds after BLE advertising period (XX:XX:05 to XX:XX:59)
7. WHEN IoT_Core is unreachable in BLE mode, THE ESP32_Device SHALL retry IoT_Core connection once per hour
8. THE ESP32_Device SHALL store IoT_Core reachability state in RTC memory to persist across deep sleep cycles
9. THE power optimization strategy SHALL prioritize minimizing WiFi on-time over connection frequency (5-minute IoT cycle vs 1-minute BLE cycle)

### Requirement 7: WiFi Connection Management

**User Story:** As a device owner, I want WiFi connected only during IoT Core sync sessions for absolute minimum duration, so that power consumption is minimized.

#### Acceptance Criteria

1. WHEN ESP32_Device wakes with IoT_Core marked as reachable, THE ESP32_Device SHALL connect to WiFi before attempting MQTT connection
2. WHEN IoT_Core synchronization session completes, THE ESP32_Device SHALL immediately disconnect WiFi before entering deep sleep to minimize WiFi on-time
3. THE ESP32_Device SHALL disconnect WiFi within 1 second of completing synchronization session to minimize power consumption
4. WHEN IoT_Core is unreachable, THE ESP32_Device SHALL disconnect WiFi after NTP_Sync to conserve power
5. THE ESP32_Device SHALL implement WiFi connection timeout of 15 seconds
6. IF WiFi connection fails, THEN THE ESP32_Device SHALL mark IoT_Core as unreachable and activate BLE_Interface
7. WHEN WiFi signal strength is below -80 dBm, THE ESP32_Device SHALL log warning but continue operation
8. THE WiFi connection duration SHALL be minimized to reduce average power consumption (target: WiFi active less than 10% of wake period)

### Requirement 8: Web Client Connection Selection

**User Story:** As a web application user, I want automatic connection method selection, so that I get the best available connection without manual intervention.

#### Acceptance Criteria

1. WHEN user clicks Connect button, THE Connection_Manager SHALL first attempt IoT_Core connection via AWS SDK
2. THE Connection_Manager SHALL wait maximum 5 seconds for IoT_Core connection attempt
3. IF IoT_Core connection succeeds, THEN THE Web_Client SHALL use Device_Shadow for all device communication (asynchronous)
4. IF IoT_Core connection fails or times out, THEN THE Connection_Manager SHALL attempt BLE connection
5. THE Web_Client SHALL display active connection method (IoT Core or BLE) in connection status indicator
6. WHEN IoT_Core connection is active, THE Web_Client SHALL subscribe to device shadow updates to receive state when device syncs

### Requirement 9: Web Client IoT Core Communication

**User Story:** As a web application user, I want to control the device via IoT Core, so that I can manage it remotely without BLE proximity.

#### Acceptance Criteria

1. WHEN Web_Client uses IoT_Core connection, THE Web_Client SHALL read device state from Device_Shadow reported section
2. WHEN user modifies settings, THE Web_Client SHALL update Device_Shadow desired section with new configuration
3. THE Web_Client SHALL display that command is pending until device next syncs and confirms changes
4. WHEN Device_Shadow reported section updates, THE Web_Client SHALL update UI to reflect new device state within 1 second
5. THE Web_Client SHALL indicate last sync time based on Device_Shadow metadata timestamp
6. THE Web_Client SHALL implement message debouncing to limit shadow desired updates to maximum 1 per second

### Requirement 10: Security and Authentication

**User Story:** As a security-conscious user, I want secure communication between device and cloud, so that my device cannot be compromised or controlled by unauthorized parties.

#### Acceptance Criteria

1. THE MQTT_Client SHALL use TLS 1.2 or higher for all IoT_Core communication
2. THE ESP32_Device SHALL validate IoT_Core server certificate against AWS root CA certificate
3. THE Certificate_Store SHALL encrypt private keys using ESP32 flash encryption when available
4. THE ESP32_Device SHALL NOT expose device certificates or private keys via BLE_Interface
5. THE Web_Client SHALL authenticate using IAM credentials (access key ID and secret access key) before allowing IoT_Core device access
6. THE IoT policy SHALL restrict device access to only the device owner's AWS account

### Requirement 11: Error Handling and Resilience

**User Story:** As a device owner, I want the device to handle connectivity issues gracefully with immediate fallback, so that it continues operating even when cloud is unavailable while minimizing wasted power on failed connections.

#### Acceptance Criteria

1. WHEN any IoT_Core connection or synchronization failure occurs, THE ESP32_Device SHALL immediately mark IoT_Core as unreachable and switch to BLE mode to avoid wasting power on repeated failed WiFi attempts
2. WHEN switching to BLE mode, THE ESP32_Device SHALL wake at XX:XX:59 each minute, advertise for 6 seconds, and sleep for 54 seconds
3. WHEN synchronization session fails to complete within 30 seconds, THE ESP32_Device SHALL abort session and immediately switch to BLE mode to minimize wasted WiFi power
4. WHEN Device_Shadow synchronization fails, THE ESP32_Device SHALL continue local operation with last known configuration
5. WHEN in BLE mode, THE ESP32_Device SHALL retry IoT_Core connection once per hour to detect when cloud becomes available
6. THE ESP32_Device SHALL log all IoT_Core errors to serial output with ERROR level for debugging
7. THE ESP32_Device SHALL disconnect WiFi immediately upon detecting any connection failure to minimize power waste

### Requirement 12: Documentation and Setup Instructions

**User Story:** As a new user, I want clear setup instructions for AWS IoT Core, so that I can configure my device without extensive AWS knowledge.

#### Acceptance Criteria

1. THE Registration_Process SHALL be documented in README-IOTCORE.md with step-by-step instructions
2. THE documentation SHALL include AWS CLI commands for creating IoT thing, generating certificates, and attaching policies
3. THE documentation SHALL provide example IoT policy JSON with minimum required permissions
4. THE documentation SHALL explain how to configure device with IoT Core endpoint and certificates
5. THE documentation SHALL include troubleshooting section for common connection issues
6. THE documentation SHALL provide example MQTT topic structure and message formats with JSON schemas

### Requirement 13: Configuration Management

**User Story:** As a developer, I want centralized IoT Core configuration, so that I can easily modify endpoints and settings.

#### Acceptance Criteria

1. THE config.h file SHALL include IoT_Core endpoint URL configuration parameter
2. THE config.h file SHALL include MQTT keep-alive interval configuration (default 60 seconds)
3. THE config.h file SHALL include IoT_Core connection timeout configuration (default 10 seconds)
4. THE config.h file SHALL include deep sleep duration for IoT mode (default 300 seconds)
5. THE config.h file SHALL include maximum message queue size configuration (default 10 messages)
6. WHERE IoT_Core is disabled at compile time, THE ESP32_Device SHALL operate in BLE-only mode

### Requirement 14: Backward Compatibility

**User Story:** As an existing user, I want my device to continue working with BLE, so that I'm not forced to use cloud connectivity.

#### Acceptance Criteria

1. WHEN IoT_Core credentials are not configured, THE ESP32_Device SHALL operate exactly as current implementation with BLE_Interface
2. WHEN IoT_Core is unreachable, THE BLE_Interface SHALL activate to provide local access fallback
3. WHEN IoT_Core is reachable, THE ESP32_Device SHALL operate in IoT-only mode without activating BLE_Interface
4. WHEN device switches from IoT mode to BLE mode due to connectivity loss, THE ESP32_Device SHALL preserve all configuration in NVS
5. THE Web_Client SHALL support devices without IoT_Core configuration by falling back to BLE-only mode
6. THE existing BLE characteristics and service UUIDs SHALL remain unchanged

### Requirement 15: Parser and Serializer Requirements

**User Story:** As a developer, I want robust JSON parsing for MQTT messages, so that malformed messages don't crash the device.

#### Acceptance Criteria

1. WHEN a valid Command_Message JSON is received, THE JSON_Parser SHALL parse it into command structure
2. WHEN an invalid Command_Message JSON is received, THE JSON_Parser SHALL return descriptive error message
3. THE JSON_Serializer SHALL format device state into valid JSON for Status_Message and Device_Shadow updates
4. FOR ALL valid device state structures, parsing then serializing then parsing SHALL produce equivalent structure (round-trip property)
5. THE JSON_Parser SHALL validate required fields are present before processing commands
6. THE JSON_Parser SHALL reject messages exceeding 4KB size limit with error response
