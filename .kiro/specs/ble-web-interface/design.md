# Design Document: BLE Web Interface

## Overview

The BLE Web Interface is a single-page web application (SPA) that uses the Web Bluetooth API to communicate with the ESP32-C3 temperature-controlled relay timer. The application provides a responsive, mobile-first interface for monitoring device status, controlling the relay, and configuring schedules and temperature settings.

The design addresses the unique challenge of intermittent connectivity due to the device's power-saving deep sleep mode (60-second wake cycles). The application uses a state management approach with operation queuing to handle disconnections gracefully and provide a seamless user experience.

## Architecture

### Technology Stack

- **React 18**: Component-based UI framework
- **React Hooks**: State management and side effects
- **CSS3**: Responsive styling with CSS Grid and Flexbox (or CSS Modules)
- **Web Bluetooth API**: Native browser BLE communication
- **Vite**: Build tool and development server

### Application Structure

```
webclient/
├── public/
│   └── index.html           # HTML entry point
├── src/
│   ├── App.jsx              # Main application component
│   ├── App.css              # Main application styles
│   ├── main.jsx             # React entry point
│   ├── hooks/
│   │   └── useBLE.js        # Custom hook for BLE operations
│   ├── utils/
│   │   └── dataFormatter.js # BLE data encoding/decoding
│   ├── components/
│   │   ├── Header.jsx       # Header with menu and connection status
│   │   ├── Sidebar.jsx      # Navigation sidebar
│   │   ├── Dashboard.jsx    # Dashboard view
│   │   ├── TimerProgramming.jsx  # Timer programming view
│   │   ├── TemperatureSettings.jsx  # Temperature settings view
│   │   ├── TemperatureDisplay.jsx   # Temperature display component
│   │   ├── RelayGauge.jsx   # Relay state gauge
│   │   ├── ScheduleGrid.jsx # Schedule editing grid
│   │   └── Notification.jsx # Notification/toast component
│   └── styles/
│       └── global.css       # Global styles
├── package.json
└── vite.config.js
```

### Architectural Patterns

**Single Page Application (SPA)**: React-based SPA with client-side routing. This eliminates page reloads and maintains BLE connection state.

**Component-Based Architecture**: React components with clear responsibilities:
- `App`: Root component, manages global state and routing
- `useBLE`: Custom hook encapsulating all BLE operations
- `dataFormatter`: Utility module for encoding/decoding
- View components: Dashboard, TimerProgramming, TemperatureSettings
- UI components: Header, Sidebar, TemperatureDisplay, RelayGauge, etc.

**Custom Hooks Pattern**: The `useBLE` custom hook encapsulates BLE logic and provides a clean API to components, promoting reusability and separation of concerns.

**Unidirectional Data Flow**: React's one-way data binding ensures predictable state management.

## Components and Interfaces

### 1. useBLE Custom Hook

**Responsibilities:**
- Discover and connect to ESP32_Device
- Read/write GATT characteristics
- Handle notifications
- Manage connection state
- Queue operations during disconnection
- Automatic reconnection logic

**Hook Interface:**

```javascript
const useBLE = () => {
  // State
  const [isConnected, setIsConnected] = useState(false);
  const [isConnecting, setIsConnecting] = useState(false);
  const [deviceData, setDeviceData] = useState({
    temperature: null,
    currentTime: null,
    relayState: null,
    schedule: null,
    thresholds: null
  });
  const [error, setError] = useState(null);
  const [operationQueue, setOperationQueue] = useState([]);
  
  // Connection management
  const connect = async () => { /* ... */ };
  const disconnect = async () => { /* ... */ };
  
  // Characteristic operations
  const readTemperature = async () => { /* ... */ };
  const readCurrentTime = async () => { /* ... */ };
  const readRelayState = async () => { /* ... */ };
  const readSchedule = async () => { /* ... */ };
  const readTemperatureThresholds = async () => { /* ... */ };
  
  const writeRelayState = async (state) => { /* ... */ };
  const writeSchedule = async (day, schedule) => { /* ... */ };
  const writeTemperatureThresholds = async (high, low) => { /* ... */ };
  const writeTemperatureCalibration = async (temp) => { /* ... */ };
  
  return {
    isConnected,
    isConnecting,
    deviceData,
    error,
    connect,
    disconnect,
    readTemperature,
    readCurrentTime,
    readRelayState,
    readSchedule,
    readTemperatureThresholds,
    writeRelayState,
    writeSchedule,
    writeTemperatureThresholds,
    writeTemperatureCalibration
  };
};
```

**Connection State Management:**

The hook uses React state to track connection status and automatically triggers re-renders when state changes. Connection state transitions:

```
[Disconnected] --connect()--> [Connecting] --success--> [Connected]
                                   |                         |
                                   |                         |
                              [Error]                  disconnect()
                                   |                         |
                                   v                         v
                              [Disconnected] <-------- [Disconnected]
```

**Reconnection Strategy:**
1. Detect disconnection via notification timeout or explicit disconnect event
2. Wait 5 seconds before first retry
3. Retry up to 3 times with exponential backoff (5s, 10s, 20s)
4. After 3 failures, set error state prompting user for manual reconnection

**Operation Queue:**
- Operations attempted while disconnected are queued in state
- Maximum queue size: 10 operations
- Operations executed in FIFO order upon reconnection using useEffect
- Queue cleared on explicit disconnect

### 2. DataFormatter Module

**Responsibilities:**
- Encode JavaScript values to BLE byte arrays
- Decode BLE byte arrays to JavaScript values
- Validate data formats

**Public Interface:**

```javascript
class DataFormatter {
  // Decoders
  static decodeTemperature(dataView)
  static decodeCurrentTime(dataView)
  static decodeSchedule(text)
  static decodeTemperatureThresholds(dataView)
  
  // Encoders
  static encodeRelayState(state)
  static encodeSchedule(day, scheduleString)
  static encodeTemperatureThresholds(high, low)
  static encodeTemperatureCalibration(temp)
  
  // Validators
  static validateScheduleString(scheduleString)
  static validateTemperatureThresholds(high, low)
  static validateDay(day)
}
```

**Data Format Specifications:**

*Temperature (0x2A6E):*
- Input: DataView (2 bytes)
- Output: Number (Celsius)
- Formula: `(byte[0] | (byte[1] << 8)) / 100.0`

*Current Time (0x2A2B):*
- Input: DataView (10 bytes)
- Output: Object `{year, month, day, hour, minute, second, dayOfWeek}`
- Format: Bluetooth SIG Current Time specification

*Schedule (0xFF05):*
- Read: Multi-line text with CRLF separators
- Output: Array of 7 strings (24 chars each)
- Write: 25 bytes (day index + 24 chars)

*Temperature Thresholds (0xFF06):*
- Input/Output: Object `{high, low}`
- Format: 4 bytes, 2x signed 16-bit little-endian

*Temperature Calibration (0xFF07):*
- Input: Number (Celsius) or -999 (reset)
- Format: 2 bytes, signed 16-bit little-endian

### 3. React Components

#### App Component

**Responsibilities:**
- Root component
- Initialize useBLE hook
- Manage view routing
- Provide global state context

```jsx
function App() {
  const ble = useBLE();
  const [currentView, setCurrentView] = useState('dashboard');
  const [sidebarOpen, setSidebarOpen] = useState(false);
  
  return (
    <div className="app">
      <Header 
        isConnected={ble.isConnected}
        onMenuClick={() => setSidebarOpen(!sidebarOpen)}
        onConnect={ble.connect}
      />
      <Sidebar 
        isOpen={sidebarOpen}
        currentView={currentView}
        onNavigate={(view) => {
          setCurrentView(view);
          setSidebarOpen(false);
        }}
      />
      <main>
        {currentView === 'dashboard' && <Dashboard ble={ble} />}
        {currentView === 'timer' && <TimerProgramming ble={ble} />}
        {currentView === 'settings' && <TemperatureSettings ble={ble} />}
      </main>
      {ble.error && <Notification type="error" message={ble.error} />}
    </div>
  );
}
```

#### Dashboard Component

**Responsibilities:**
- Display current device status
- Show temperature, time, relay state, schedule, thresholds
- Provide manual relay control

```jsx
function Dashboard({ ble }) {
  const { deviceData, isConnected, writeRelayState } = ble;
  
  const handleRelayToggle = async (newState) => {
    try {
      await writeRelayState(newState);
      // Show success notification
    } catch (error) {
      // Show error notification
    }
  };
  
  return (
    <div className="dashboard">
      <TemperatureDisplay temperature={deviceData.temperature} />
      <RelayGauge 
        state={deviceData.relayState}
        onToggle={handleRelayToggle}
        disabled={!isConnected}
      />
      <div className="thresholds">
        <div>High: {deviceData.thresholds?.high}°C</div>
        <div>Low: {deviceData.thresholds?.low}°C</div>
      </div>
      <div className="today-schedule">
        {/* Display today's schedule */}
      </div>
    </div>
  );
}
```

#### TimerProgramming Component

**Responsibilities:**
- Display 7x24 schedule grid
- Allow editing schedule
- Save schedule to device

```jsx
function TimerProgramming({ ble }) {
  const [schedule, setSchedule] = useState(ble.deviceData.schedule || []);
  const [modified, setModified] = useState(false);
  
  const handleCellClick = (day, hour) => {
    // Cycle through H -> L -> O -> H
    const newSchedule = [...schedule];
    const currentMode = newSchedule[day][hour];
    const nextMode = currentMode === 'H' ? 'L' : currentMode === 'L' ? 'O' : 'H';
    newSchedule[day] = newSchedule[day].substring(0, hour) + nextMode + newSchedule[day].substring(hour + 1);
    setSchedule(newSchedule);
    setModified(true);
  };
  
  const handleSave = async () => {
    try {
      for (let day = 0; day < 7; day++) {
        await ble.writeSchedule(day, schedule[day]);
      }
      setModified(false);
      // Show success notification
    } catch (error) {
      // Show error notification
    }
  };
  
  return (
    <div className="timer-programming">
      <ScheduleGrid 
        schedule={schedule}
        onCellClick={handleCellClick}
      />
      {modified && (
        <button onClick={handleSave}>Save Schedule</button>
      )}
    </div>
  );
}
```

#### TemperatureSettings Component

**Responsibilities:**
- Display and edit temperature thresholds
- Provide calibration interface
- Save settings to device

```jsx
function TemperatureSettings({ ble }) {
  const [high, setHigh] = useState(ble.deviceData.thresholds?.high || 22);
  const [low, setLow] = useState(ble.deviceData.thresholds?.low || 18);
  const [calibration, setCalibration] = useState('');
  
  const handleSaveThresholds = async () => {
    if (high <= low) {
      // Show validation error
      return;
    }
    try {
      await ble.writeTemperatureThresholds(high, low);
      // Show success notification
    } catch (error) {
      // Show error notification
    }
  };
  
  const handleCalibrate = async () => {
    try {
      await ble.writeTemperatureCalibration(parseFloat(calibration));
      setCalibration('');
      // Show success notification
    } catch (error) {
      // Show error notification
    }
  };
  
  const handleResetCalibration = async () => {
    try {
      await ble.writeTemperatureCalibration(-999);
      // Show success notification
    } catch (error) {
      // Show error notification
    }
  };
  
  return (
    <div className="temperature-settings">
      <div className="thresholds-section">
        <label>
          High Temperature (°C):
          <input 
            type="number" 
            value={high} 
            onChange={(e) => setHigh(parseFloat(e.target.value))}
          />
        </label>
        <label>
          Low Temperature (°C):
          <input 
            type="number" 
            value={low} 
            onChange={(e) => setLow(parseFloat(e.target.value))}
          />
        </label>
        <button onClick={handleSaveThresholds}>Save Thresholds</button>
      </div>
      
      <div className="calibration-section">
        <label>
          Calibration Temperature (°C):
          <input 
            type="number" 
            value={calibration} 
            onChange={(e) => setCalibration(e.target.value)}
          />
        </label>
        <button onClick={handleCalibrate}>Calibrate</button>
        <button onClick={handleResetCalibration}>Reset Calibration</button>
      </div>
    </div>
  );
}
```

### 4. Component Interaction Flow

**Initialization Flow:**

```
1. App component mounts
2. useBLE hook initializes
3. App renders Dashboard (default view)
4. User clicks connect button
5. useBLE.connect() called
6. BLE connection established
7. useBLE reads initial data (temp, time, relay, schedule, thresholds)
8. deviceData state updated
9. All components re-render with new data
```

**User Interaction Flow (Toggle Relay):**

```
1. User clicks relay toggle in Dashboard
2. Dashboard calls handleRelayToggle(newState)
3. handleRelayToggle calls ble.writeRelayState(newState)
4. useBLE writes to characteristic 0xFF02
5. Write succeeds
6. useBLE updates deviceData.relayState
7. Dashboard re-renders with new relay state
8. Success notification shown
```

**Disconnection Flow:**

```
1. Device enters deep sleep
2. BLE connection lost
3. useBLE detects disconnection
4. isConnected state set to false
5. All components re-render (controls disabled)
6. Disconnected status shown in Header
7. User attempts operation
8. Operation queued in operationQueue state
9. Device wakes up
10. useBLE reconnects automatically
11. useEffect executes queued operations
12. Components re-render with updated data
```

## Data Models

### ConnectionState

```javascript
{
  status: 'disconnected' | 'connecting' | 'connected',
  device: BluetoothDevice | null,
  server: BluetoothRemoteGATTServer | null,
  service: BluetoothRemoteGATTService | null,
  characteristics: Map<UUID, BluetoothRemoteGATTCharacteristic>,
  lastConnected: Date | null,
  reconnectAttempts: number,
  operationQueue: Array<Operation>
}
```

### DeviceData

```javascript
{
  temperature: number | null,           // Celsius
  currentTime: {
    year: number,
    month: number,
    day: number,
    hour: number,
    minute: number,
    second: number,
    dayOfWeek: number                   // 1=Mon, 7=Sun
  } | null,
  relayState: boolean | null,           // true=ON, false=OFF
  schedule: Array<string> | null,       // 7 strings of 24 chars
  thresholds: {
    high: number,                       // Celsius
    low: number                         // Celsius
  } | null
}
```

### Operation (for queue)

```javascript
{
  type: 'read' | 'write',
  characteristic: UUID,
  value: any | null,                    // null for reads
  timestamp: Date,
  resolve: Function,
  reject: Function
}
```

### ScheduleCell

```javascript
{
  day: number,                          // 0-6 (Mon-Sun)
  hour: number,                         // 0-23
  mode: 'H' | 'L' | 'O'
}
```

### Notification

```javascript
{
  id: string,
  type: 'success' | 'error' | 'info' | 'loading',
  message: string,
  duration: number | null,              // null for persistent
  timestamp: Date
}
```

## Error Handling

### Error Categories

**1. Browser Compatibility Errors**
- Web Bluetooth API not supported
- HTTPS required (Web Bluetooth requires secure context)
- Browser version too old

**2. Permission Errors**
- User denied Bluetooth permission
- Bluetooth adapter not available
- Bluetooth disabled on device

**3. Connection Errors**
- Device not found (out of range or sleeping)
- Connection timeout
- Connection lost unexpectedly
- GATT service discovery failed

**4. Operation Errors**
- Characteristic read failed
- Characteristic write failed
- Invalid data format
- Notification setup failed

**5. Validation Errors**
- Invalid schedule format
- Invalid temperature thresholds (High ≤ Low)
- Invalid calibration value

### Error Handling Strategy

**Browser Compatibility:**
```javascript
if (!navigator.bluetooth) {
  UIController.showError(
    'Web Bluetooth not supported. Please use Chrome, Edge, or Opera.'
  );
  // Disable connect button
}
```

**Permission Errors:**
```javascript
try {
  await navigator.bluetooth.requestDevice(...);
} catch (error) {
  if (error.name === 'NotFoundError') {
    UIController.showError('No device selected');
  } else if (error.name === 'SecurityError') {
    UIController.showError('Bluetooth access denied. Check permissions.');
  }
}
```

**Connection Errors:**
```javascript
// Timeout for device not found
const SCAN_TIMEOUT = 10000; // 10 seconds
setTimeout(() => {
  if (!connected) {
    UIController.showError(
      'Device not found. The device wakes every 60 seconds. Please try again.'
    );
  }
}, SCAN_TIMEOUT);

// Handle unexpected disconnection
device.addEventListener('gattserverdisconnected', () => {
  BLEManager.handleDisconnection();
  UIController.showDisconnected(estimateNextWake());
});
```

**Operation Errors:**
```javascript
try {
  await characteristic.writeValue(data);
  UIController.showSuccess('Settings saved');
} catch (error) {
  console.error('Write failed:', error);
  UIController.showError('Failed to save settings. Please try again.');
  // Revert UI to previous state
}
```

**Validation Errors:**
```javascript
if (highTemp <= lowTemp) {
  UIController.showError('High temperature must be greater than low temperature');
  return;
}

if (!/^[HLO]{24}$/.test(scheduleString)) {
  UIController.showError('Invalid schedule format');
  return;
}
```

### Error Recovery

**Automatic Recovery:**
- Connection lost: Attempt automatic reconnection (3 tries)
- Operation failed: Retry once after 1 second
- Notification stopped: Re-enable notifications

**User-Initiated Recovery:**
- Manual reconnect button after auto-reconnect fails
- Refresh button to reload data
- Reset button to clear local state

**Graceful Degradation:**
- Show last known values when disconnected
- Disable controls that require connection
- Queue operations for later execution

## Testing Strategy

The testing strategy employs both unit tests and property-based tests to ensure comprehensive coverage and correctness.

### Unit Testing

Unit tests will verify specific examples, edge cases, and integration points:

**BLEManager Tests:**
- Connection establishment with mock BluetoothDevice
- Disconnection handling
- Operation queuing when disconnected
- Reconnection logic with exponential backoff
- Notification setup and handling
- Error handling for various failure scenarios

**DataFormatter Tests:**
- Temperature decoding with known values (e.g., [0x64, 0x08] → 21.48°C)
- Current time decoding with known byte arrays
- Schedule parsing with CRLF separators
- Threshold encoding/decoding round-trip
- Calibration value encoding (including -999 reset)
- Edge cases: empty strings, invalid formats, boundary values

**UIController Tests:**
- View switching
- Dashboard component updates
- Notification display and auto-dismiss
- Form validation
- Responsive layout behavior
- Event handler registration

**App Integration Tests:**
- End-to-end flow: connect → read data → update UI
- User interaction flow: toggle relay → write → update UI
- Error flow: connection fails → show error → retry
- Disconnection flow: detect → queue operations → reconnect → execute queue

### Property-Based Testing

Property-based tests will be implemented using **fast-check** (JavaScript property testing library). Each test will run a minimum of 100 iterations to ensure comprehensive input coverage.


## Correctness Properties

A property is a characteristic or behavior that should hold true across all valid executions of a system—essentially, a formal statement about what the system should do. Properties serve as the bridge between human-readable specifications and machine-verifiable correctness guarantees.

### Property 1: Temperature Threshold Validation

*For any* pair of temperature values (high, low), the validation function should reject the pair if and only if high ≤ low.

**Validates: Requirements 6.3**

**Rationale:** Temperature thresholds must maintain the invariant that the high threshold is strictly greater than the low threshold. This property ensures the validation logic correctly enforces this constraint across all possible input combinations.

### Property 2: Operation Queuing During Disconnection

*For any* BLE operation attempted while disconnected, the operation should be added to the operation queue and not executed immediately.

**Validates: Requirements 7.3**

**Rationale:** When the device is disconnected (sleeping), operations must be queued for later execution rather than failing immediately. This property ensures the queuing mechanism works for all operation types.

### Property 3: Queued Operation Execution on Reconnection

*For any* non-empty operation queue, when the BLE connection is re-established, all queued operations should be executed in FIFO order.

**Validates: Requirements 7.4**

**Rationale:** Queued operations must be executed in the order they were queued to maintain operation semantics. This property ensures the queue execution logic preserves order for any sequence of operations.

### Property 4: Temperature Data Round-Trip Consistency

*For any* valid temperature value in Celsius (integer from -100 to 100), encoding the value to bytes and then decoding should produce the original value.

**Validates: Requirements 8.1, 8.7**

**Rationale:** Temperature encoding and decoding must be inverse operations. This round-trip property ensures data integrity for temperature readings and calibration values across the full valid range.

### Property 5: Threshold Data Round-Trip Consistency

*For any* valid pair of threshold values (high, low) where high > low, encoding to bytes and then decoding should produce equivalent values.

**Validates: Requirements 8.4, 8.6**

**Rationale:** Threshold encoding and decoding must preserve the original values. This round-trip property ensures data integrity for temperature threshold configuration.

### Property 6: Current Time Decoding Correctness

*For any* valid 10-byte Bluetooth SIG Current Time format byte array, decoding should produce a valid time object with year ≥ 2000, month in 1-12, day in 1-31, hour in 0-23, minute in 0-59, second in 0-59, and dayOfWeek in 1-7.

**Validates: Requirements 8.2**

**Rationale:** Time decoding must produce valid time components that respect calendar constraints. This property ensures the decoder correctly interprets the Bluetooth SIG format and produces valid time objects.

### Property 7: Schedule Parsing Correctness

*For any* valid schedule string (7 lines of 24 characters each, separated by CRLF, each character being H, L, or O), parsing should produce an array of 7 strings with 24 characters each.

**Validates: Requirements 8.3**

**Rationale:** Schedule parsing must correctly split the multi-line format and preserve all schedule data. This property ensures the parser handles the CRLF-separated format correctly for all valid schedules.

### Property 8: Schedule Encoding Format

*For any* valid day index (0-6) and schedule string (24 characters of H, L, or O), encoding should produce exactly 25 bytes with the first byte equal to the day index and the remaining 24 bytes matching the schedule string characters.

**Validates: Requirements 8.5**

**Rationale:** Schedule encoding must follow the exact format expected by the device. This property ensures the encoder produces correctly formatted data for all valid schedule inputs.

### Property 9: Loading Indicator Display

*For any* BLE operation (read or write), while the operation is in progress, a loading indicator should be visible in the UI.

**Validates: Requirements 10.1**

**Rationale:** Users need feedback that operations are in progress. This property ensures loading indicators are consistently shown for all BLE operations, providing a consistent user experience.

### Property 10: Error Message Display

*For any* BLE operation that fails, an error message should be displayed to the user.

**Validates: Requirements 10.3**

**Rationale:** Users need feedback when operations fail. This property ensures error messages are consistently shown for all operation failures, enabling users to understand and respond to errors.

