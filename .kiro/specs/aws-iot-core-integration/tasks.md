# Implementation Plan: AWS IoT Core Integration

## Overview

This implementation adds AWS IoT Core cloud connectivity to the ESP32-C3 timer device with power-optimized synchronization. The device connects briefly every 5 minutes to sync state via Device Shadow, then returns to deep sleep. BLE fallback provides local access when IoT Core is unreachable.

Key implementation phases:
1. ESP32 certificate storage and IoT Core connectivity
2. Device Shadow synchronization and command processing
3. Connection mode management (IoT vs BLE)
4. Web client IoT Core integration
5. Property-based testing for 39 correctness properties

## Tasks

- [x] 1. Set up certificate storage infrastructure
  - [x] 1.1 Create cert_store module for NVS-based certificate management
    - Implement cert_store_init(), cert_store_has_credentials()
    - Implement cert_store_get_device_cert(), cert_store_get_private_key(), cert_store_get_root_ca()
    - Implement cert_store_set_credentials() for provisioning
    - Use NVS namespace "iot_creds" with keys: iot_cert, iot_key, iot_ca
    - Support PEM format for all certificates
    - Add flash encryption support when available
    - _Requirements: 1.1, 1.2, 10.3_
  
  - [ ]* 1.2 Write property test for certificate storage round-trip
    - **Property 1: Certificate Storage Round-Trip**
    - **Validates: Requirements 1.1**
    - Generate random certificate data, store, retrieve, verify equivalence
    - Minimum 100 iterations
  
  - [ ]* 1.3 Write unit tests for cert_store module
    - Test missing credentials detection
    - Test invalid PEM format handling
    - Test buffer overflow protection
    - _Requirements: 1.1, 1.2, 1.3_

- [x] 2. Implement IoT Core MQTT connectivity
  - [x] 2.1 Create iot_manager module for AWS IoT Core communication
    - Implement iot_manager_init() to load credentials from cert_store
    - Implement MQTT client with TLS 1.2 mutual authentication
    - Configure AWS IoT Core endpoint from config.h
    - Implement connection timeout (10 seconds)
    - Implement sync session timeout (30 seconds)
    - _Requirements: 2.1, 2.2, 2.7, 10.1, 10.2_
  
  - [x] 2.2 Implement MQTT connection establishment
    - Implement iot_manager_sync_session() for brief sync sessions
    - Subscribe to command topic: esp32timer/{deviceId}/commands/#
    - Publish connection status to: esp32timer/{deviceId}/status/connection
    - Return ESP_OK on success, ESP_FAIL on failure for mode switching
    - _Requirements: 2.1, 2.3, 2.4, 2.6_
  
  - [ ]* 2.3 Write property tests for MQTT connection behavior
    - **Property 4: Connection Sequencing** - NTP sync triggers MQTT attempt
    - **Property 5: Connection Timeout Triggers Fallback** - 10-second timeout switches to BLE
    - **Property 6: Minimal Connection Duration** - Connection only during sync (max 30s)
    - **Property 7: Command Topic Subscription** - Subscribe to commands/# during session
    - **Validates: Requirements 2.1, 2.4, 2.5, 2.6**
  
  - [ ]* 2.4 Write unit tests for MQTT connection
    - Test successful connection flow
    - Test connection timeout handling
    - Test TLS handshake failure
    - Test certificate validation failure
    - _Requirements: 2.1, 2.2, 2.4_

- [x] 3. Implement Device Shadow synchronization
  - [x] 3.1 Implement shadow state publishing
    - Implement iot_manager_publish_state() to publish reported section
    - Include battery, temperature, relay_state, timestamp, schedule_mode, thresholds, schedule, calibration
    - Use Device Shadow update API
    - Complete within sync session before disconnecting
    - _Requirements: 3.1, 3.2, 3.5_
  
  - [x] 3.2 Implement shadow delta processing
    - Implement iot_manager_process_delta() to receive desired state changes
    - Subscribe to shadow delta topic during sync session
    - Parse delta JSON for override, schedule, thresholds, calibration commands
    - Apply changes to device state and NVS
    - Publish confirmation 
    before disconnecting
    - _Requirements: 3.3, 3.4, 5.1, 5.2, 5.3, 5.4_
  
  - [ ]* 3.3 Write property tests for Device Shadow operations
    - **Property 8: Shadow Contains Required Fields** - Verify all required fields present
    - **Property 9: Delta Processing Updates Reported State** - Delta application updates reported
    - **Property 10: Sync Failure Triggers Immediate Fallback** - Any failure switches to BLE
    - **Property 13: Delta Batching** - Multiple deltas batched into single update
    - **Property 14: Invalid Delta Handling** - Invalid delta logged, processing continues
    - **Validates: Requirements 3.2, 3.4, 3.8, 5.5, 5.7**
  
  - [ ]* 3.4 Write unit tests for shadow synchronization
    - Test successful shadow publish
    - Test shadow update timeout
    - Test delta processing with valid commands
    - Test delta processing with invalid format
    - Test multiple delta batching
    - _Requirements: 3.1, 3.3, 3.4, 3.8_

- [x] 4. Checkpoint - Ensure IoT Core connectivity tests pass
  - Ensure all tests pass, ask the user if questions arise.

- [x] 5. Implement status message publishing
  - [x] 5.1 Create status message JSON serializer
    - Implement function to serialize device state to JSON
    - Include version, device_id, timestamp, battery, temperature, relay_state, schedule_mode, wifi_rssi, uptime_seconds
    - Validate JSON output format
    - _Requirements: 4.2, 4.6_
  
  - [x] 5.2 Implement status message publishing
    - Publish to topic: esp32timer/{deviceId}/status/telemetry
    - Publish during sync session before disconnecting
    - Complete within 2 seconds to minimize WiFi power
    - Publish every 5 minutes in IoT mode, every 60 minutes in BLE mode with hourly retry
    - _Requirements: 4.1, 4.3, 4.4, 4.5, 4.7_
  
  - [ ]* 5.3 Write property tests for status messages
    - **Property 11: Status Message Contains Required Fields** - Verify all required fields
    - **Property 12: Status Message JSON Validity** - Valid JSON with schema version
    - **Validates: Requirements 4.2, 4.6**
  
  - [ ]* 5.4 Write unit tests for status message publishing
    - Test JSON serialization with various device states
    - Test publish success
    - Test publish failure handling
    - Test timing constraint (2-second completion)
    - _Requirements: 4.1, 4.2, 4.7_

- [x] 6. Implement connection mode management
  - [x] 6.1 Create connection_manager module for mode selection
    - Implement connection_manager_init() to read RTC state
    - Implement connection_manager_get_mode() to determine IoT vs BLE mode
    - Implement connection_manager_get_sleep_duration_sec() for mode-specific sleep
    - Implement connection_manager_update_mode() to switch modes based on sync result
    - Implement connection_manager_should_retry_iot() for hourly retry in BLE mode
    - Store reachability state in RTC memory and NVS
    - _Requirements: 6.8, 11.1_
  
  - [x] 6.2 Implement IoT mode behavior
    - Wake every 300 seconds (5 minutes)
    - Attempt sync session via iot_manager_sync_session()
    - On success: disconnect WiFi immediately, sleep 5 minutes
    - On failure: mark unreachable, switch to BLE mode
    - Disable BLE interface during IoT mode
    - _Requirements: 6.1, 6.2, 6.3, 6.4, 14.3_
  
  - [x] 6.3 Implement BLE mode behavior
    - Wake at XX:XX:59 each minute
    - Advertise BLE for exactly 6 seconds (XX:XX:59 to XX:XX:05)
    - Sleep for 54 seconds (XX:XX:05 to XX:XX:59)
    - Retry IoT Core connection once per hour
    - Disconnect WiFi after NTP sync when in BLE mode
    - _Requirements: 6.5, 6.6, 6.7, 7.4, 11.2, 11.5_
  
  - [ ]* 6.4 Write property tests for connection mode management
    - **Property 2: BLE Fallback on Missing Credentials** - No credentials → BLE-only mode
    - **Property 15: IoT Mode Wake Frequency** - IoT mode wakes every 300 seconds
    - **Property 16: Immediate Disconnect After Sync** - Disconnect within 1 second after sync
    - **Property 17: BLE Disabled in IoT Mode** - BLE not activated in IoT mode
    - **Property 18: BLE Mode Timing** - BLE mode: wake XX:XX:59, advertise 6s, sleep 54s
    - **Property 19: Hourly IoT Retry in BLE Mode** - Retry IoT once per hour in BLE mode
    - **Property 20: Reachability State Persistence** - RTC state persists across deep sleep
    - **Validates: Requirements 1.3, 6.1, 6.2, 6.4, 6.5, 6.6, 6.7, 6.8, 7.2, 7.3, 14.1**
  
  - [ ]* 6.5 Write unit tests for connection manager
    - Test mode selection logic
    - Test sleep duration calculation
    - Test mode switching on sync success/failure
    - Test hourly retry timing
    - Test RTC memory persistence
    - _Requirements: 6.1, 6.7, 6.8_

- [x] 7. Implement WiFi connection management
  - [x] 7.1 Update wifi_manager for IoT Core integration
    - Connect WiFi before MQTT when IoT Core is reachable
    - Implement immediate disconnect after sync (within 1 second)
    - Implement WiFi connection timeout (15 seconds)
    - Disconnect WiFi after NTP sync when in BLE mode
    - Log warning for weak signal (<-80 dBm) but continue
    - _Requirements: 7.1, 7.2, 7.3, 7.4, 7.5, 7.7_
  
  - [ ]* 7.2 Write property tests for WiFi management
    - **Property 21: WiFi Connection Sequencing** - WiFi before MQTT in IoT mode
    - **Property 22: WiFi Disconnect After NTP in BLE Mode** - Disconnect after NTP in BLE
    - **Property 23: WiFi Connection Timeout** - 15-second timeout triggers BLE mode
    - **Property 24: Weak Signal Warning** - Log warning for RSSI < -80 dBm, continue
    - **Validates: Requirements 7.1, 7.4, 7.5, 7.6, 7.7**
  
  - [ ]* 7.3 Write unit tests for WiFi management
    - Test WiFi connection success
    - Test WiFi connection timeout
    - Test immediate disconnect after sync
    - Test disconnect after NTP in BLE mode
    - Test weak signal warning
    - _Requirements: 7.1, 7.2, 7.4, 7.5, 7.7_

- [x] 8. Update main.c for dual-mode operation
  - [x] 8.1 Integrate IoT Core into boot sequence
    - Initialize cert_store, connection_manager, iot_manager
    - Determine connection mode (IoT vs BLE)
    - Implement IoT mode flow: WiFi → NTP → MQTT sync → disconnect → sleep 5 min
    - Implement BLE mode flow: WiFi → NTP → disconnect → BLE 6s → sleep 54s
    - Implement hourly IoT retry in BLE mode
    - Handle mode switching on sync success/failure
    - _Requirements: 1.2, 1.3, 6.1, 6.2, 6.3, 6.5, 6.7_
  
  - [ ]* 8.2 Write integration tests for boot sequence
    - Test complete IoT mode flow
    - Test complete BLE mode flow
    - Test mode switching on failure
    - Test hourly retry in BLE mode
    - _Requirements: 6.1, 6.2, 6.3, 6.7_

- [x] 9. Add configuration parameters to config.h
  - [x] 9.1 Add IoT Core configuration section
    - Add IOT_CORE_ENABLED flag
    - Add IOT_ENDPOINT URL
    - Add IOT_MQTT_PORT (8883)
    - Add IOT_KEEP_ALIVE_SEC (60)
    - Add IOT_CONNECTION_TIMEOUT_SEC (10)
    - Add IOT_SYNC_TIMEOUT_SEC (30)
    - Add IOT_DEEP_SLEEP_DURATION_SEC (300)
    - Add IOT_RETRY_INTERVAL_SEC (3600)
    - Add detailed comments for each parameter
    - _Requirements: 13.1, 13.2, 13.3, 13.4, 13.6_

- [x] 10. Checkpoint - Ensure ESP32 firmware integration complete
  - Ensure all tests pass, ask the user if questions arise.

- [x] 11. Implement JSON parsing and serialization
  - [x] 11.1 Create JSON parser for command messages
    - Parse override commands (command, duration_minutes)
    - Parse schedule commands (command, day, hours)
    - Parse threshold commands (command, high, low)
    - Parse calibration commands (command, temp_points, battery_points)
    - Validate required fields are present
    - Return descriptive error for invalid JSON
    - Reject messages exceeding 4KB
    - _Requirements: 15.1, 15.2, 15.5, 15.6_
  
  - [x] 11.2 Create JSON serializer for device state
    - Serialize device state to Status Message format
    - Serialize device state to Device Shadow format
    - Ensure valid JSON output with schema version
    - _Requirements: 15.3_
  
  - [ ]* 11.3 Write property tests for JSON operations
    - **Property 34: JSON Parsing Success** - Valid JSON parses successfully
    - **Property 35: JSON Parsing Error Handling** - Invalid JSON returns error
    - **Property 36: JSON Serialization Validity** - Serializer produces valid JSON
    - **Property 37: JSON Round-Trip Property** - Serialize → parse → serialize produces equivalent output
    - **Property 38: JSON Field Validation** - Parser validates required fields
    - **Property 39: JSON Size Limit** - Messages >4KB rejected
    - **Validates: Requirements 15.1, 15.2, 15.3, 15.4, 15.5, 15.6**
  
  - [ ]* 11.4 Write unit tests for JSON operations
    - Test parsing valid command messages
    - Test parsing invalid JSON
    - Test missing required fields
    - Test oversized messages
    - Test serialization of various device states
    - _Requirements: 15.1, 15.2, 15.5, 15.6_

- [x] 12. Implement error handling and logging
  - [x] 12.1 Add comprehensive error handling to iot_manager
    - Handle MQTT connection errors (timeout, TLS failure, certificate error)
    - Handle shadow operation errors (publish failure, update timeout)
    - Handle WiFi disconnect errors
    - Immediate fallback to BLE mode on any error
    - Immediate WiFi disconnect on any error to minimize power waste
    - Log all errors at ERROR level with descriptive messages
    - _Requirements: 11.1, 11.3, 11.6, 11.7_
  
  - [ ]* 12.2 Write property tests for error handling
    - **Property 10: Sync Failure Triggers Immediate Fallback** - Any failure switches to BLE
    - **Property 31: Configuration Preservation Across Mode Switch** - Config preserved in NVS
    - **Property 32: Error Logging** - All errors logged at ERROR level
    - **Validates: Requirements 3.8, 11.1, 11.3, 11.4, 11.6, 11.7**
  
  - [ ]* 12.3 Write unit tests for error handling
    - Test connection timeout handling
    - Test TLS handshake failure
    - Test shadow publish failure
    - Test sync timeout handling
    - Test WiFi disconnect failure
    - Test configuration preservation
    - _Requirements: 11.1, 11.3, 11.4, 11.7_
- [x] 13. Install AWS SDK dependencies for web client
  - [x] 13.1 Add AWS SDK packages to package.jsonient
  - [ ] 13.1 Add AWS SDK packages to package.json
    - Install @aws-sdk/client-iot-data-plane
    - Install @aws-sdk/credential-providers
    - Update package.json and package-lock.json
    - _Requirements: 9.1, 10.5_
- [x] 14. Create useIoT hook for web client
  - [x] 14.1 Implement useIoT hook for IoT Core communication
  - [ ] 14.1 Implement useIoT hook for IoT Core communication
    - Configure IoTDataPlaneClient with IAM credentials
    - Implement connect() to establish IoT connection
    - Implement disconnect() to close IoT connection
    - Implement getDeviceShadow() to read reported state
    - Implement updateDeviceShadow() to write desired state
    - Subscribe to shadow updates (poll every 5 seconds)
    - Extract lastSyncTime from shadow metadata
    - Detect pending commands by comparing reported vs desired
    - Implement error handling with retry logic
    - _Requirements: 8.1, 8.2, 9.1, 9.2, 9.4, 9.5_
  - [x] 14.2 Implement shadow update debouncing
  - [ ] 14.2 Implement shadow update debouncing
    - Debounce desired state updates to max 1 per second
    - Batch multiple setting changes into single update
    - _Requirements: 9.6_
  
  - [ ]* 14.3 Write property tests for useIoT hook
    - **Property 25: Web Client Connection Priority** - IoT first, 5s timeout, BLE fallback
    - **Property 26: Shadow-Based Communication in IoT Mode** - Read reported, write desired
    - **Property 27: Shadow Update Triggers UI Update** - UI updates within 1s of shadow update
    - **Property 28: Last Sync Time Display** - Extract from shadow metadata
    - **Property 29: Shadow Update Debouncing** - Max 1 update per second
    - **Validates: Requirements 8.1, 8.2, 8.3, 9.1, 9.2, 9.4, 9.5, 9.6**
  
  - [ ]* 14.4 Write unit tests for useIoT hook
    - Test successful IoT connection
    - Test connection timeout
    - Test shadow read operation
    - Test shadow update operation
    - Test shadow update debouncing
    - Test pending command detection
    - Test error handling
    - _Requirements: 8.1, 8.2, 9.1, 9.2, 9.6_
- [x] 15. Update useBLEUnified for IoT-first connection pattern
  - [x] 15.1 Modify connection logic to try IoT Core firstttern
  - [ ] 15.1 Modify connection logic to try IoT Core first
    - Attempt IoT Core connection with 5-second timeout
    - Fall back to BLE if IoT fails or times out
    - Display active connection method in UI
    - _Requirements: 8.1, 8.2, 8.3, 8.4_
  
  - [ ]* 15.2 Write unit tests for connection fallback
    - Test IoT success path
    - Test IoT timeout → BLE fallback
    - Test IoT failure → BLE fallback
    - Test connection method display
    - _Requirements: 8.1, 8.2, 8.4_
- [x] 16. Update UI components for IoT Core integration
  - [x] 16.1 Update connection status indicatoregration
  - [ ] 16.1 Update connection status indicator
    - Display "Cloud Connected" for IoT Core
    - Display "Local Connected" for BLE
    - Display "Connecting..." during connection attempt
    - Display "Disconnected" when not connected
    - _Requirements: 8.5_
  - [x] 16.2 Add last sync time indicator
  - [ ] 16.2 Add last sync time indicator
    - Display timestamp of last device sync
    - Format as relative time (e.g., "2 minutes ago")
    - Update when shadow metadata changes
    - _Requirements: 9.5_
  - [x] 16.3 Add pending commands indicator
  - [ ] 16.3 Add pending commands indicator
    - Display "Pending" badge when desired != reported
    - Show tooltip explaining device will apply on next sync
    - Clear badge when reported matches desired
    - _Requirements: 9.3_
  
  - [ ]* 16.4 Write unit tests for UI components
    - Test connection status display
    - Test last sync time formatting
    - Test pending commands indicator
    - _Requirements: 8.5, 9.3, 9.5_

- [x] 17. Implement IAM credential configuration
  - [x] 17.1 Add IAM credential input to web client
    - Create configuration form for AWS_ACCESS_KEY_ID and AWS_SECRET_ACCESS_KEY
    - Store credentials securely (environment variables or secure storage)
    - Validate credentials before allowing IoT connection
    - Display authentication errors clearly
    - _Requirements: 10.5_
  
  - [ ]* 17.2 Write unit tests for credential management
    - Test credential validation
    - Test authentication error handling
    - Test secure storage
    - _Requirements: 10.5_

- [ ] 18. Checkpoint - Ensure web client integration complete
  - Ensure all tests pass, ask the user if questions arise.

- [ ] 19. Implement backward compatibility features
  - [ ] 19.1 Ensure BLE-only mode works without IoT credentials
    - Device operates exactly as current implementation when credentials missing
    - Web client falls back to BLE-only when IoT unavailable
    - BLE service UUIDs and characteristics unchanged
    - _Requirements: 1.3, 14.1, 14.2, 14.5, 14.6_
  
  - [ ]* 19.2 Write property tests for backward compatibility
    - **Property 2: BLE Fallback on Missing Credentials** - No credentials → BLE-only
    - **Property 33: BLE Compatibility** - BLE UUIDs unchanged
    - **Validates: Requirements 1.3, 14.1, 14.6**
  
  - [ ]* 19.3 Write integration tests for backward compatibility
    - Test device without IoT credentials
    - Test web client BLE fallback
    - Test BLE characteristics unchanged
    - _Requirements: 14.1, 14.2, 14.5, 14.6_

- [ ] 20. Implement security features
  - [ ] 20.1 Ensure certificate security
    - Validate IoT Core server certificate against AWS root CA
    - Use TLS 1.2 or higher for all MQTT communication
    - Enable flash encryption for private keys when available
    - Ensure certificates not exposed via BLE interface
    - _Requirements: 10.1, 10.2, 10.3, 10.4_
  
  - [ ]* 20.2 Write property test for certificate security
    - **Property 30: Certificate Security via BLE** - Certificates not exposed via BLE
    - **Validates: Requirements 10.4**
  
  - [ ]* 20.3 Write unit tests for security features
    - Test TLS 1.2 enforcement
    - Test server certificate validation
    - Test flash encryption support
    - Test BLE characteristic access restrictions
    - _Requirements: 10.1, 10.2, 10.3, 10.4_

- [ ] 21. Create MQTT topic structure validation
  - [ ] 21.1 Implement topic pattern validation
    - Validate all topics follow pattern: esp32timer/{deviceId}/{message_type}
    - Validate status/connection topic
    - Validate status/telemetry topic
    - Validate commands/# subscription pattern
    - _Requirements: 1.5_
  
  - [ ]* 21.2 Write property test for topic pattern compliance
    - **Property 3: MQTT Topic Pattern Compliance** - All topics match pattern
    - **Validates: Requirements 1.5**
  
  - [ ]* 21.3 Write unit tests for topic validation
    - Test topic generation for various device IDs
    - Test topic pattern matching
    - _Requirements: 1.5_

- [ ] 22. Create README-IOTCORE.md documentation
  - [ ] 22.1 Write AWS IoT Core setup instructions
    - Document device registration process
    - Provide AWS CLI commands for creating IoT thing
    - Provide AWS CLI commands for generating certificates
    - Provide AWS CLI commands for attaching policies
    - Include example IoT policy JSON with minimum permissions
    - Document how to configure device with endpoint and certificates
    - Include troubleshooting section for common issues
    - Provide example MQTT topic structure and message formats
    - Include JSON schemas for all message types
    - _Requirements: 12.1, 12.2, 12.3, 12.4, 12.5, 12.6_

- [ ] 23. Final integration and validation
  - [ ] 23.1 Test complete IoT Core sync session flow
    - Test device wake → WiFi → MQTT → shadow sync → disconnect → sleep
    - Measure sync session duration (target: 5-30 seconds)
    - Measure WiFi on-time percentage (target: <10% of wake period)
    - Validate power consumption estimates
    - _Requirements: 6.2, 6.3, 7.8_
  
  - [ ] 23.2 Test IoT Core to BLE fallback
    - Test immediate fallback on connection failure
    - Test immediate fallback on sync timeout
    - Test hourly retry in BLE mode
    - Test mode switching back to IoT when available
    - _Requirements: 11.1, 11.2, 11.3, 11.5_
  
  - [ ] 23.3 Test web client end-to-end flows
    - Test IoT Core connection and shadow operations
    - Test IoT to BLE fallback
    - Test pending commands indicator
    - Test last sync time display
    - _Requirements: 8.1, 8.2, 8.3, 8.4, 9.3, 9.5_

- [ ] 24. Final checkpoint - Ensure all tests pass
  - Ensure all tests pass, ask the user if questions arise.

## Notes

- Tasks marked with `*` are optional and can be skipped for faster MVP
- Each task references specific requirements for traceability
- Checkpoints ensure incremental validation at key milestones
- Property tests validate 39 universal correctness properties from design
- Unit tests validate specific examples and edge cases
- Integration tests validate complete flows across components
- Power optimization is critical: minimize WiFi on-time throughout implementation
- Security is paramount: TLS 1.2, certificate validation, no credential exposure
- Backward compatibility ensures existing BLE functionality unchanged
