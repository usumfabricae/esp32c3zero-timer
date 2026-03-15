# Design Document: AWS IoT Core Integration

## Overview

This design document specifies the integration of AWS IoT Core cloud connectivity into the ESP32-C3 temperature-controlled timer device. The integration adds remote device management and control capabilities while maintaining backward compatibility with the existing BLE interface.

The system uses a disconnected, periodic synchronization architecture optimized for power efficiency:
- Device briefly connects to AWS IoT Core every 5 minutes (IoT mode)
- Synchronizes state via Device Shadow (reported/desired pattern)
- Receives and processes pending commands
- Immediately disconnects and returns to deep sleep
- Falls back to BLE mode (1-minute wake cycles) when IoT Core is unreachable

Power optimization is achieved by minimizing WiFi on-time. WiFi consumes more power than BLE but provides cloud connectivity. The trade-off is to connect less frequently (5 minutes vs 1 minute) but for very brief periods (5-30 seconds maximum), resulting in lower average power consumption than frequent BLE wake cycles.

The web client supports both connection methods:
- IoT Core connection: Remote access via AWS SDK and Device Shadow (asynchronous)
- BLE connection: Local access via Web Bluetooth API (direct)

Authentication uses IAM credentials for simplicity in this single-user application.

## Architecture

### System Components

```
┌─────────────────────────────────────────────────────────────────┐
│                         AWS Cloud                                │
│  ┌──────────────┐    ┌─────────────────┐    ┌────────────────┐ │
│  │  IoT Core    │◄───┤  Device Shadow  │───►│  Web Client    │ │
│  │  MQTT Broker │    │  (State Sync)   │    │  (AWS SDK v3)  │ │
│  └──────────────┘    └─────────────────┘    └────────────────┘ │
│         ▲                                            │           │
└─────────┼────────────────────────────────────────────┼───────────┘
          │ MQTT/TLS                          IAM Auth │
          │ (5-min sync)                                │
          │                                             │
┌─────────┴─────────────────────────────────────────────┼───────────┐
│                    ESP32-C3 Device                     │           │
│  ┌────────────────┐  ┌──────────────┐  ┌────────────┐│           │
│  │ Connection Mgr │  │  MQTT Client │  │ BLE Server │◄───────────┤
│  │ (Mode Select)  │  │  (AWS IoT)   │  │ (Fallback) ││  BLE      │
│  └────────────────┘  └──────────────┘  └────────────┘│  (Local)  │
│         │                    │                 │      │           │
│  ┌──────┴────────────────────┴─────────────────┴────┐│           │
│  │         Device State & Control Logic              ││           │
│  │  (Scheduler, Relay, Sensors, NVS Storage)         ││           │
│  └───────────────────────────────────────────────────┘│           │
└────────────────────────────────────────────────────────┴───────────┘
```

### Connection Modes

**IoT Mode (Cloud Connected):**
- Wake every 300 seconds (5 minutes)
- Connect WiFi → Connect MQTT → Sync Shadow → Disconnect → Deep Sleep
- Sync duration: 5-30 seconds (variable based on operations)
- BLE disabled during IoT mode
- Immediate fallback to BLE mode on any failure

**BLE Mode (Local Fallback):**
- Wake at XX:XX:59 each minute
- Advertise BLE for 6 seconds (XX:XX:59 to XX:XX:05)
- Deep sleep for 54 seconds (XX:XX:05 to XX:XX:59)
- Retry IoT Core connection once per hour
- WiFi only for NTP sync (disconnected after sync)

### Power Optimization Strategy

The design prioritizes minimizing WiFi on-time:
1. WiFi active only during sync sessions (target: <10% of wake period)
2. Immediate disconnect after sync completion
3. Abort and fallback if sync exceeds 30 seconds
4. Less frequent connections (5 min) compensate for higher WiFi power
5. BLE fallback provides reliability with lower power radio

## Components and Interfaces

### ESP32 Firmware Components

#### 1. IoT Core Manager (`iot_manager.c/h`)

Manages AWS IoT Core connectivity and synchronization sessions.

**Responsibilities:**
- MQTT connection establishment with TLS 1.2
- Device Shadow synchronization (reported/desired)
- Status message publishing
- Command processing from shadow deltas
- Connection timeout and error handling
- Immediate WiFi disconnect after sync

**Key Functions:**
```c
// Initialize IoT Core manager with credentials from NVS
esp_err_t iot_manager_init(void);

// Attempt brief sync session (connect, sync, disconnect)
// Returns ESP_OK if sync successful, ESP_FAIL if should fallback to BLE
esp_err_t iot_manager_sync_session(void);

// Publish device state to shadow reported section
esp_err_t iot_manager_publish_state(const device_state_t *state);

// Process shadow delta updates (desired state changes)
esp_err_t iot_manager_process_delta(void);

// Check if IoT Core is marked as reachable
bool iot_manager_is_reachable(void);

// Mark IoT Core as unreachable (triggers BLE mode)
void iot_manager_mark_unreachable(void);

// Cleanup and disconnect
void iot_manager_deinit(void);
```

**Configuration (config.h):**
```c
#define IOT_CORE_ENABLED 1
#define IOT_ENDPOINT "xxxxx-ats.iot.region.amazonaws.com"
#define IOT_MQTT_PORT 8883
#define IOT_KEEP_ALIVE_SEC 60
#define IOT_CONNECTION_TIMEOUT_SEC 10
#define IOT_SYNC_TIMEOUT_SEC 30
#define IOT_DEEP_SLEEP_DURATION_SEC 300  // 5 minutes
#define IOT_RETRY_INTERVAL_SEC 3600      // 1 hour
```

#### 2. Connection Manager (`connection_manager.c/h`)

Selects and manages active connection mode (IoT or BLE).

**Responsibilities:**
- Mode selection based on IoT Core reachability
- RTC memory persistence of reachability state
- Coordinating wake behavior (5-min vs 1-min cycles)
- Triggering IoT retry attempts in BLE mode

**Key Functions:**
```c
// Initialize connection manager (reads RTC state)
void connection_manager_init(void);

// Determine which mode to use this wake cycle
connection_mode_t connection_manager_get_mode(void);

// Calculate next sleep duration based on mode
uint32_t connection_manager_get_sleep_duration_sec(void);

// Update mode based on sync result
void connection_manager_update_mode(bool iot_success);

// Check if this wake cycle should retry IoT (hourly in BLE mode)
bool connection_manager_should_retry_iot(void);
```

**Types:**
```c
typedef enum {
    CONNECTION_MODE_IOT,   // Cloud connected (5-min cycle)
    CONNECTION_MODE_BLE    // Local fallback (1-min cycle)
} connection_mode_t;
```

#### 3. Certificate Store (`cert_store.c/h`)

Manages AWS IoT Core device certificates and private keys in NVS.

**Responsibilities:**
- Secure storage of device certificate, private key, root CA
- Loading credentials for MQTT client
- Validation of certificate presence
- Flash encryption support when available

**Key Functions:**
```c
// Initialize certificate store
esp_err_t cert_store_init(void);

// Check if certificates are provisioned
bool cert_store_has_credentials(void);

// Load certificate for MQTT client
esp_err_t cert_store_get_device_cert(char *buf, size_t buf_len);
esp_err_t cert_store_get_private_key(char *buf, size_t buf_len);
esp_err_t cert_store_get_root_ca(char *buf, size_t buf_len);

// Store certificates (used during provisioning)
esp_err_t cert_store_set_credentials(
    const char *device_cert,
    const char *private_key,
    const char *root_ca
);
```

**NVS Keys:**
- `iot_cert`: Device certificate (PEM format)
- `iot_key`: Private key (PEM format)
- `iot_ca`: AWS root CA certificate (PEM format)

#### 4. Modified Main Loop (`main.c`)

Updated boot sequence to support IoT Core integration.

**Boot Sequence:**
```
1. Initialize NVS, WiFi, GPIO, sensors
2. Initialize certificate store
3. Initialize connection manager (read RTC state)
4. Determine connection mode
5. Connect WiFi
6. Perform NTP sync
7. If IoT mode:
   a. Attempt sync session (iot_manager_sync_session)
   b. If success: disconnect WiFi, sleep 5 minutes
   c. If failure: mark unreachable, switch to BLE mode
8. If BLE mode:
   a. Disconnect WiFi (keep only for NTP)
   b. Check if hourly retry time
   c. If retry: attempt sync session, update mode
   d. Activate BLE for 6 seconds
   e. Sleep 54 seconds (or 5 min if retry succeeded)
9. Enter deep sleep with calculated duration
```

### Web Client Components

#### 1. AWS SDK for JavaScript Interface

The web client uses AWS SDK for JavaScript v3 to interact with IoT Core.

**Authentication with IAM Credentials:**
```javascript
import { IoTDataPlaneClient, GetThingShadowCommand, UpdateThingShadowCommand } 
  from "@aws-sdk/client-iot-data-plane";
import { fromEnv } from "@aws-sdk/credential-providers";

// Configure AWS SDK with IAM credentials
// Credentials can be provided via:
// 1. Environment variables (AWS_ACCESS_KEY_ID, AWS_SECRET_ACCESS_KEY)
// 2. Configuration file
// 3. Direct configuration (not recommended for production)

const iotClient = new IoTDataPlaneClient({
  region: "us-east-1",  // Your AWS region
  credentials: {
    accessKeyId: process.env.AWS_ACCESS_KEY_ID,
    secretAccessKey: process.env.AWS_SECRET_ACCESS_KEY
  },
  endpoint: "https://xxxxx-ats.iot.us-east-1.amazonaws.com"
});

// Get device shadow (read current state)
async function getDeviceShadow(thingName) {
  const command = new GetThingShadowCommand({ thingName });
  const response = await iotClient.send(command);
  const shadow = JSON.parse(new TextDecoder().decode(response.payload));
  return shadow.state.reported;
}

// Update device shadow (send commands)
async function updateDeviceShadow(thingName, desiredState) {
  const payload = {
    state: {
      desired: desiredState
    }
  };
  const command = new UpdateThingShadowCommand({
    thingName,
    payload: JSON.stringify(payload)
  });
  await iotClient.send(command);
}
```

**IAM User Permissions:**

The IAM user needs the following IoT Core permissions:
```json
{
  "Version": "2012-10-17",
  "Statement": [
    {
      "Effect": "Allow",
      "Action": [
        "iot:Connect",
        "iot:Publish",
        "iot:Subscribe",
        "iot:Receive",
        "iot:GetThingShadow",
        "iot:UpdateThingShadow"
      ],
      "Resource": [
        "arn:aws:iot:region:account-id:thing/esp32timer-*",
        "arn:aws:iot:region:account-id:topic/esp32timer/*"
      ]
    }
  ]
}
```

#### 2. useIoT Hook

Custom React hook for IoT Core communication.

**Responsibilities:**
- Authenticates via IAM credentials
- Reads device state from shadow reported section
- Writes commands to shadow desired section
- Subscribes to shadow updates
- Provides connection status
- Handles errors and timeouts

**Interface:**
```javascript
const {
  connected,        // boolean: IoT Core connection status
  deviceState,      // object: current device state from shadow
  lastSyncTime,     // Date: timestamp of last shadow update
  pendingCommands,  // boolean: true if desired != reported
  connect,          // function: establish IoT connection
  disconnect,       // function: close IoT connection
  sendCommand,      // function: update shadow desired state
  error             // string: error message if any
} = useIoT(thingName);
```

**Implementation Details:**
- Uses AWS SDK v3 IoT Data Plane client
- Polls shadow every 5 seconds when connected
- Debounces desired state updates (max 1/second)
- Compares reported vs desired to detect pending commands
- Extracts lastSyncTime from shadow metadata

#### 3. Connection Manager Component

UI component for selecting connection method.

**Features:**
- "Connect" button attempts IoT Core first
- 5-second timeout for IoT attempt
- Automatic fallback to BLE if IoT fails
- Visual indicator of active connection method
- Status messages for connection state

**Connection Flow:**
```
User clicks "Connect"
  ↓
Try IoT Core (5 sec timeout)
  ↓
Success? → Use IoT Core (show "Cloud Connected")
  ↓
Failure? → Try BLE
  ↓
Success? → Use BLE (show "Local Connected")
  ↓
Failure? → Show error
```

## Data Models

### Device Shadow Structure

The Device Shadow follows AWS IoT Core's standard structure with reported and desired sections.

**Complete Shadow Document:**
```json
{
  "state": {
    "reported": {
      "battery": 85,
      "temperature": 21.5,
      "relay_state": "H",
      "timestamp": 1704067200,
      "schedule_mode": "H",
      "thresholds": {
        "high": 22.0,
        "low": 18.0
      },
      "schedule": {
        "monday": "HHHHHHLLLLLLLLLLLLLHHHHH",
        "tuesday": "HHHHHHLLLLLLLLLLLLLHHHHH",
        "wednesday": "HHHHHHLLLLLLLLLLLLLHHHHH",
        "thursday": "HHHHHHLLLLLLLLLLLLLHHHHH",
        "friday": "HHHHHHLLLLLLLLLLLLLHHHHH",
        "saturday": "OOOOOOOOOOOOOOOOOOOOOOOO",
        "sunday": "OOOOOOOOOOOOOOOOOOOOOOOO"
      },
      "calibration": {
        "temp_points": [[0, 0], [1024, 100]],
        "battery_points": [[0, 0], [4095, 100]]
      }
    },
    "desired": {
      "override": {
        "active": true,
        "duration_minutes": 60
      },
      "thresholds": {
        "high": 23.0,
        "low": 19.0
      }
    }
  },
  "metadata": {
    "reported": {
      "battery": { "timestamp": 1704067200 },
      "temperature": { "timestamp": 1704067200 }
    }
  },
  "version": 42,
  "timestamp": 1704067200
}
```

**Field Descriptions:**

*Reported Section (Device → Cloud):*
- `battery`: Battery percentage (0-100)
- `temperature`: Temperature in Celsius
- `relay_state`: Current relay state ("H", "L", or "O")
- `timestamp`: Unix timestamp from device
- `schedule_mode`: Current schedule mode for this hour
- `thresholds`: Temperature thresholds for H/L modes
- `schedule`: Weekly schedule (7 days × 24 characters)
- `calibration`: Sensor calibration points

*Desired Section (Cloud → Device):*
- `override`: Manual override command
- `thresholds`: New temperature thresholds
- `schedule`: New weekly schedule
- `calibration`: New calibration parameters

### Status Message Format

Published to `esp32timer/{deviceId}/status/telemetry` during sync sessions.

```json
{
  "version": "1.0",
  "device_id": "esp32timer-abc123",
  "timestamp": 1704067200,
  "battery": 85,
  "temperature": 21.5,
  "relay_state": "H",
  "schedule_mode": "H",
  "wifi_rssi": -65,
  "uptime_seconds": 3600
}
```

### Command Message Format (Legacy MQTT)

While Device Shadow is the primary command mechanism, direct MQTT commands are supported on topic `esp32timer/{deviceId}/commands/#`.

**Override Command:**
```json
{
  "command": "override",
  "duration_minutes": 60
}
```

**Schedule Update:**
```json
{
  "command": "schedule",
  "day": "monday",
  "hours": "HHHHHHLLLLLLLLLLLLLHHHHH"
}
```

**Threshold Update:**
```json
{
  "command": "thresholds",
  "high": 23.0,
  "low": 19.0
}
```

### NVS Storage Schema

**IoT Core Credentials:**
- Namespace: `iot_creds`
- Keys: `iot_cert`, `iot_key`, `iot_ca`
- Format: PEM-encoded strings

**IoT Core State:**
- Namespace: `iot_state`
- Key: `reachable` (uint8_t: 1=reachable, 0=unreachable)
- Key: `last_retry` (uint32_t: Unix timestamp of last retry)

**RTC Memory:**
- `iot_reachable` (uint8_t): Persists across deep sleep
- `wake_count` (uint32_t): Counts wake cycles for hourly retry


## Correctness Properties

*A property is a characteristic or behavior that should hold true across all valid executions of a system-essentially, a formal statement about what the system should do. Properties serve as the bridge between human-readable specifications and machine-verifiable correctness guarantees.*

### Property Reflection

After analyzing all acceptance criteria, I identified several areas of redundancy:

1. **Timing properties**: Multiple criteria specify timing constraints (connection timeout, sync duration, disconnect timing). These can be consolidated into fewer comprehensive properties.

2. **Mode switching**: Multiple criteria describe switching to BLE mode on failure. These can be combined into a single comprehensive error handling property.

3. **Shadow operations**: Multiple criteria describe shadow read/write/update operations. These can be consolidated into properties about shadow synchronization.

4. **Configuration properties**: Multiple criteria about config.h content are examples, not properties requiring runtime testing.

5. **Documentation properties**: All documentation criteria are examples to verify once, not properties.

The following properties represent the unique, testable behaviors after eliminating redundancy:

### Property 1: Certificate Storage Round-Trip

*For any* valid device certificate, private key, and root CA, storing them in Certificate_Store and then retrieving them SHALL produce equivalent credentials.

**Validates: Requirements 1.1**

### Property 2: BLE Fallback on Missing Credentials

*For any* device state where IoT Core credentials are not configured, the device SHALL operate in BLE-only mode with 1-minute wake cycles.

**Validates: Requirements 1.3, 14.1**

### Property 3: MQTT Topic Pattern Compliance

*For any* device ID and message type, generated MQTT topics SHALL match the pattern `esp32timer/{deviceId}/{message_type}`.

**Validates: Requirements 1.5**

### Property 4: Connection Sequencing

*For any* successful NTP sync, the device SHALL attempt MQTT connection before entering deep sleep.

**Validates: Requirements 2.1**

### Property 5: Connection Timeout Triggers Fallback

*For any* MQTT connection attempt that exceeds 10 seconds, the device SHALL immediately mark IoT Core as unreachable and switch to BLE mode.

**Validates: Requirements 2.4, 11.1**

### Property 6: Minimal Connection Duration

*For any* synchronization session, the MQTT connection SHALL be maintained only for the duration needed to complete sync operations (maximum 30 seconds).

**Validates: Requirements 2.5, 3.5, 6.3**

### Property 7: Command Topic Subscription

*For any* established MQTT connection, the device SHALL subscribe to topic `esp32timer/{deviceId}/commands/#` for the duration of the session.

**Validates: Requirements 2.6**

### Property 8: Shadow Contains Required Fields

*For any* Device Shadow published by the device, it SHALL contain battery level, temperature, relay state, timestamp, schedule, and thresholds in the reported section.

**Validates: Requirements 3.2**

### Property 9: Delta Processing Updates Reported State

*For any* Device Shadow delta notification received, applying the changes SHALL result in the reported section being updated with the new values before disconnecting.

**Validates: Requirements 3.4, 5.1, 5.2, 5.3, 5.4**

### Property 10: Sync Failure Triggers Immediate Fallback

*For any* synchronization session failure (connection, shadow update, or timeout), the device SHALL immediately mark IoT Core as unreachable, disconnect WiFi, and switch to BLE mode.

**Validates: Requirements 3.8, 11.1, 11.3, 11.7**

### Property 11: Status Message Contains Required Fields

*For any* Status Message published by the device, it SHALL include battery percentage, temperature, relay state, Unix timestamp, and schedule mode.

**Validates: Requirements 4.2**

### Property 12: Status Message JSON Validity

*For any* Status Message, it SHALL be valid JSON with a schema version field.

**Validates: Requirements 4.6**

### Property 13: Delta Batching

*For any* synchronization session with multiple pending delta updates, all confirmations SHALL be batched into a single shadow update before disconnecting.

**Validates: Requirements 5.5, 5.6**

### Property 14: Invalid Delta Handling

*For any* delta update with invalid format, the device SHALL log an error and continue processing remaining deltas without applying the invalid change.

**Validates: Requirements 5.7**

### Property 15: IoT Mode Wake Frequency

*For any* device in IoT mode (IoT Core reachable), wake cycles SHALL occur every 300 seconds (5 minutes).

**Validates: Requirements 6.1**

### Property 16: Immediate Disconnect After Sync

*For any* successful synchronization session, the device SHALL disconnect MQTT and WiFi immediately (within 1 second) before entering deep sleep.

**Validates: Requirements 6.2, 7.2, 7.3**

### Property 17: BLE Disabled in IoT Mode

*For any* device in IoT mode (IoT Core reachable), the BLE interface SHALL NOT be activated during sleep cycles.

**Validates: Requirements 6.4, 14.3**

### Property 18: BLE Mode Timing

*For any* device in BLE mode (IoT Core unreachable), the device SHALL wake at XX:XX:59 each minute, advertise BLE for exactly 6 seconds, and sleep for 54 seconds.

**Validates: Requirements 6.5, 6.6, 11.2**

### Property 19: Hourly IoT Retry in BLE Mode

*For any* device in BLE mode, IoT Core connection SHALL be retried once per hour.

**Validates: Requirements 6.7, 11.5**

### Property 20: Reachability State Persistence

*For any* deep sleep cycle, the IoT Core reachability state stored in RTC memory SHALL persist and be available after wake.

**Validates: Requirements 6.8**

### Property 21: WiFi Connection Sequencing

*For any* wake cycle with IoT Core marked as reachable, WiFi connection SHALL be established before MQTT connection is attempted.

**Validates: Requirements 7.1**

### Property 22: WiFi Disconnect After NTP in BLE Mode

*For any* device in BLE mode, WiFi SHALL disconnect after NTP sync to conserve power.

**Validates: Requirements 7.4**

### Property 23: WiFi Connection Timeout

*For any* WiFi connection attempt, if connection does not succeed within 15 seconds, the device SHALL mark IoT Core as unreachable and activate BLE interface.

**Validates: Requirements 7.5, 7.6**

### Property 24: Weak Signal Warning

*For any* WiFi connection with signal strength below -80 dBm, the device SHALL log a warning but continue operation.

**Validates: Requirements 7.7**

### Property 25: Web Client Connection Priority

*For any* user-initiated connection attempt, the Web Client SHALL attempt IoT Core connection first with a 5-second timeout before falling back to BLE.

**Validates: Requirements 8.1, 8.2, 8.4**

### Property 26: Shadow-Based Communication in IoT Mode

*For any* successful IoT Core connection, the Web Client SHALL use Device Shadow for all device communication (read from reported, write to desired).

**Validates: Requirements 8.3, 9.1, 9.2**

### Property 27: Shadow Update Triggers UI Update

*For any* Device Shadow reported section update, the Web Client SHALL update the UI to reflect new device state within 1 second.

**Validates: Requirements 9.4**

### Property 28: Last Sync Time Display

*For any* Device Shadow, the Web Client SHALL extract and display the last sync time from the shadow metadata timestamp.

**Validates: Requirements 9.5**

### Property 29: Shadow Update Debouncing

*For any* sequence of user setting changes, the Web Client SHALL debounce shadow desired updates to maximum 1 per second.

**Validates: Requirements 9.6**

### Property 30: Certificate Security via BLE

*For any* BLE characteristic read operation, device certificates and private keys SHALL NOT be exposed or accessible.

**Validates: Requirements 10.4**

### Property 31: Configuration Preservation Across Mode Switch

*For any* mode switch from IoT to BLE due to connectivity loss, all device configuration SHALL be preserved in NVS.

**Validates: Requirements 11.4, 14.4**

### Property 32: Error Logging

*For any* IoT Core error (connection, sync, timeout), the device SHALL log the error to serial output at ERROR level.

**Validates: Requirements 11.6**

### Property 33: BLE Compatibility

*For any* device with IoT Core integration, the BLE service UUIDs and characteristic UUIDs SHALL remain unchanged from the original implementation.

**Validates: Requirements 14.6**

### Property 34: JSON Parsing Success

*For any* valid Command Message JSON, the JSON parser SHALL successfully parse it into a command structure.

**Validates: Requirements 15.1**

### Property 35: JSON Parsing Error Handling

*For any* invalid Command Message JSON, the JSON parser SHALL return a descriptive error message without crashing.

**Validates: Requirements 15.2**

### Property 36: JSON Serialization Validity

*For any* device state structure, the JSON serializer SHALL produce valid JSON output.

**Validates: Requirements 15.3**

### Property 37: JSON Round-Trip Property

*For any* valid device state structure, serializing to JSON then parsing then serializing again SHALL produce equivalent JSON output.

**Validates: Requirements 15.4**

### Property 38: JSON Field Validation

*For any* Command Message JSON, the parser SHALL validate that all required fields are present before processing.

**Validates: Requirements 15.5**

### Property 39: JSON Size Limit

*For any* Command Message exceeding 4KB, the parser SHALL reject it with an error response.

**Validates: Requirements 15.6**

## Error Handling

### ESP32 Device Error Handling

**Connection Errors:**
- MQTT connection timeout (10 seconds): Mark IoT unreachable, switch to BLE mode
- WiFi connection timeout (15 seconds): Mark IoT unreachable, switch to BLE mode
- TLS handshake failure: Mark IoT unreachable, switch to BLE mode
- Certificate validation failure: Mark IoT unreachable, switch to BLE mode

**Synchronization Errors:**
- Shadow publish failure: Mark IoT unreachable, switch to BLE mode
- Shadow update timeout (30 seconds): Abort session, mark IoT unreachable, switch to BLE mode
- Delta processing error: Log error, continue with remaining deltas
- Invalid delta format: Log error, skip invalid delta, continue processing

**Power Management Errors:**
- WiFi disconnect failure: Force disconnect, log error, continue to sleep
- Deep sleep entry failure: Log error, retry sleep entry
- RTC memory corruption: Reset to default state (BLE mode)

**Error Recovery:**
- All errors trigger immediate WiFi disconnect to minimize power waste
- IoT Core errors switch to BLE mode for local access fallback
- Hourly retry attempts in BLE mode to detect when IoT Core becomes available
- All errors logged at ERROR level for debugging

### Web Client Error Handling

**IoT Core Connection Errors:**
- AWS SDK connection timeout (5 seconds): Fall back to BLE connection
- IAM authentication failure: Display error, prompt for credentials
- Shadow access denied: Display error, check IAM permissions
- Network error: Display error, retry with exponential backoff

**Shadow Operation Errors:**
- GetThingShadow failure: Display error, retry up to 3 times
- UpdateThingShadow failure: Display error, queue update for retry
- Shadow parse error: Display error, log invalid shadow format
- Shadow version conflict: Retry with latest version

**BLE Connection Errors:**
- BLE not available: Display error, suggest IoT Core connection
- Device not found: Display error, suggest checking device is awake
- Connection timeout: Display error, retry connection
- Characteristic read/write failure: Display error, reconnect

**User Feedback:**
- All errors displayed in notification component
- Connection status indicator shows current state
- Pending commands indicator shows when desired != reported
- Last sync time shows when device last connected

## Testing Strategy

### Dual Testing Approach

This feature requires both unit testing and property-based testing for comprehensive coverage:

**Unit Tests:**
- Specific examples of connection flows (successful sync, timeout, failure)
- Edge cases (empty credentials, invalid JSON, oversized messages)
- Integration points (WiFi → MQTT → Shadow sequence)
- Error conditions (network failures, certificate errors, timeouts)
- Configuration validation (config.h parameters, NVS storage)
- Documentation verification (README sections, examples, schemas)

**Property Tests:**
- Universal properties across all inputs (see Correctness Properties section)
- Comprehensive input coverage through randomization
- Minimum 100 iterations per property test
- Each test tagged with feature name and property number

### Property-Based Testing Configuration

**Library Selection:**
- ESP32 firmware: Unity test framework with custom property test harness
- Web client: fast-check (JavaScript property-based testing library)

**Test Configuration:**
- Minimum 100 iterations per property test (due to randomization)
- Each property test references its design document property
- Tag format: `Feature: aws-iot-core-integration, Property {number}: {property_text}`

**Example Property Test (ESP32):**
```c
// Feature: aws-iot-core-integration, Property 1: Certificate Storage Round-Trip
TEST_CASE("Certificate storage round-trip", "[iot][property]")
{
    for (int i = 0; i < 100; i++) {
        // Generate random certificate data
        char cert[2048], key[2048], ca[2048];
        generate_random_cert(cert, sizeof(cert));
        generate_random_key(key, sizeof(key));
        generate_random_ca(ca, sizeof(ca));
        
        // Store and retrieve
        TEST_ASSERT_EQUAL(ESP_OK, cert_store_set_credentials(cert, key, ca));
        
        char cert_out[2048], key_out[2048], ca_out[2048];
        TEST_ASSERT_EQUAL(ESP_OK, cert_store_get_device_cert(cert_out, sizeof(cert_out)));
        TEST_ASSERT_EQUAL(ESP_OK, cert_store_get_private_key(key_out, sizeof(key_out)));
        TEST_ASSERT_EQUAL(ESP_OK, cert_store_get_root_ca(ca_out, sizeof(ca_out)));
        
        // Verify equivalence
        TEST_ASSERT_EQUAL_STRING(cert, cert_out);
        TEST_ASSERT_EQUAL_STRING(key, key_out);
        TEST_ASSERT_EQUAL_STRING(ca, ca_out);
    }
}
```

**Example Property Test (Web Client):**
```javascript
// Feature: aws-iot-core-integration, Property 37: JSON Round-Trip Property
import fc from 'fast-check';

test('JSON serialization round-trip', () => {
  fc.assert(
    fc.property(
      fc.record({
        battery: fc.integer(0, 100),
        temperature: fc.float(-40, 85),
        relay_state: fc.constantFrom('H', 'L', 'O'),
        timestamp: fc.integer(0, 2147483647),
        schedule_mode: fc.constantFrom('H', 'L', 'O')
      }),
      (deviceState) => {
        const json1 = JSON.stringify(deviceState);
        const parsed = JSON.parse(json1);
        const json2 = JSON.stringify(parsed);
        expect(json1).toEqual(json2);
      }
    ),
    { numRuns: 100 }
  );
});
```

### Unit Testing Balance

Unit tests focus on:
- Specific examples demonstrating correct behavior
- Integration points between components (WiFi → MQTT → Shadow)
- Edge cases (missing credentials, invalid JSON, timeouts)
- Error conditions (network failures, certificate errors)

Property tests focus on:
- Universal properties holding for all inputs
- Comprehensive input coverage through randomization
- Round-trip properties (serialization, storage)
- Timing and sequencing properties

Together, unit tests catch concrete bugs while property tests verify general correctness across the input space.

### Test Coverage Goals

**ESP32 Firmware:**
- 90%+ code coverage for IoT Core components
- 100% coverage of error handling paths
- All 39 correctness properties implemented as property tests
- Integration tests for complete sync session flow

**Web Client:**
- 90%+ code coverage for useIoT hook
- 100% coverage of connection fallback logic
- Property tests for shadow operations and debouncing
- Integration tests for IoT → BLE fallback

### Testing Infrastructure

**ESP32 Testing:**
- Unity test framework for unit tests
- Custom property test harness for randomized testing
- Mock MQTT client for testing without AWS connection
- Mock WiFi for testing connection failures
- NVS emulation for testing storage operations

**Web Client Testing:**
- Jest for unit tests
- fast-check for property-based tests
- Mock AWS SDK for testing without AWS credentials
- Mock Web Bluetooth API for testing BLE fallback
- React Testing Library for component tests

### Continuous Integration

**Automated Testing:**
- Run all tests on every commit
- Fail build if any property test fails
- Fail build if code coverage drops below threshold
- Run property tests with increased iterations (1000) on release builds

**Test Reporting:**
- Property test failures include failing example from library
- Coverage reports highlight untested code paths
- Performance metrics track sync session duration
- Power consumption estimates from timing measurements

