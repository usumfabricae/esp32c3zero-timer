import { useState, useCallback, useEffect } from 'react';
import DataFormatter from '../utils/dataFormatter.js';

// BLE Service and Characteristic UUIDs
const SERVICE_UUID = 0x00FF;
const BATTERY_SERVICE_UUID = 0x180F;
const CHAR_TEMPERATURE = 0x2A6E;
const CHAR_CURRENT_TIME = 0x2A2B;
const CHAR_RELAY_STATE = 0xFF02;
const CHAR_SCHEDULE = 0xFF05;
const CHAR_TEMP_THRESHOLDS = 0xFF06;
const CHAR_TEMP_CALIBRATION = 0xFF07;
const CHAR_WIFI_SSID = 0xFF08;
const CHAR_WIFI_PASSWORD = 0xFF09;
const CHAR_BLE_PASSKEY = 0xFF0A;
const CHAR_BATTERY_LEVEL = 0x2A19;

export const useBLE = () => {
  const [isConnected, setIsConnected] = useState(false);
  const [isConnecting, setIsConnecting] = useState(false);
  const [error, setError] = useState(null);
  const [device, setDevice] = useState(null);
  const [server, setServer] = useState(null);
  const [characteristics, setCharacteristics] = useState({});
  const [deviceData, setDeviceData] = useState({
    temperature: null,
    currentTime: null,
    relayState: null,
    relayOverrideEndTime: null,
    schedule: null,
    thresholds: null,
    batteryLevel: null,
    batteryVoltage: null,
    wifiSsid: null
  });
  const [operationQueue, setOperationQueue] = useState([]);
  const [reconnectAttempts, setReconnectAttempts] = useState(0);

  // Automatic reconnection logic with exponential backoff
  const attemptReconnection = useCallback(async (bleDevice) => {
    const MAX_RECONNECT_ATTEMPTS = 3;
    const BACKOFF_DELAYS = [5000, 10000, 20000]; // 5s, 10s, 20s

    if (reconnectAttempts >= MAX_RECONNECT_ATTEMPTS) {
      console.error('[BLE] Max reconnection attempts reached');
      setError('Connection lost. Please reconnect manually.');
      setReconnectAttempts(0);
      return;
    }

    const delay = BACKOFF_DELAYS[reconnectAttempts] || 20000;
    console.log(`[BLE] Attempting reconnection in ${delay / 1000}s (attempt ${reconnectAttempts + 1}/${MAX_RECONNECT_ATTEMPTS})...`);
    
    setReconnectAttempts(prev => prev + 1);

    setTimeout(async () => {
      try {
        console.log('[BLE] Reconnecting to GATT server...');
        const gattServer = await bleDevice.gatt.connect();
        console.log('[BLE] Reconnected to GATT server');

        // Rediscover service and characteristics
        const service = await gattServer.getPrimaryService(SERVICE_UUID);
        const chars = {};
        
        const getChar = async (uuid, name) => {
          try {
            const char = await service.getCharacteristic(uuid);
            return char;
          } catch (err) {
            console.warn(`[BLE] ${name} not available:`, err.message);
            return null;
          }
        };

        chars[CHAR_TEMPERATURE] = await getChar(CHAR_TEMPERATURE, 'Temperature');
        chars[CHAR_CURRENT_TIME] = await getChar(CHAR_CURRENT_TIME, 'Current Time');
        chars[CHAR_RELAY_STATE] = await getChar(CHAR_RELAY_STATE, 'Relay State');
        chars[CHAR_SCHEDULE] = await getChar(CHAR_SCHEDULE, 'Schedule');
        chars[CHAR_TEMP_THRESHOLDS] = await getChar(CHAR_TEMP_THRESHOLDS, 'Temperature Thresholds');
        chars[CHAR_TEMP_CALIBRATION] = await getChar(CHAR_TEMP_CALIBRATION, 'Temperature Calibration');
        chars[CHAR_WIFI_SSID] = await getChar(CHAR_WIFI_SSID, 'WiFi SSID');
        chars[CHAR_WIFI_PASSWORD] = await getChar(CHAR_WIFI_PASSWORD, 'WiFi Password');
        chars[CHAR_BLE_PASSKEY] = await getChar(CHAR_BLE_PASSKEY, 'BLE Passkey');

        // Get Battery Service
        try {
          const batteryService = await gattServer.getPrimaryService(BATTERY_SERVICE_UUID);
          const batteryChar = await batteryService.getCharacteristic(CHAR_BATTERY_LEVEL);
          chars[CHAR_BATTERY_LEVEL] = batteryChar;
        } catch (err) {
          console.warn('[BLE] Battery Service not available:', err.message);
        }

        setServer(gattServer);
        setCharacteristics(chars);
        setIsConnected(true);
        setReconnectAttempts(0);
        setError(null);

        console.log('[BLE] Reconnection successful');
      } catch (err) {
        console.error('[BLE] Reconnection failed:', err);
        // Will trigger another reconnection attempt via the disconnect event
      }
    }, delay);
  }, [reconnectAttempts]);

  const connect = useCallback(async () => {
    setIsConnecting(true);
    setError(null);

    try {
      console.log('[BLE] Requesting device...');
      
      // Try to reconnect to a previously paired device first
      let bleDevice = null;
      let shouldTryPairedDevice = false;
      
      if (navigator.bluetooth.getDevices) {
        console.log('[BLE] Checking for previously paired devices...');
        try {
          const devices = await navigator.bluetooth.getDevices();
          console.log(`[BLE] Found ${devices.length} previously paired device(s)`);
          
          // Find our device by name
          const pairedDevice = devices.find(d => d.name === 'ESP32C3_Timer');
          
          if (pairedDevice && pairedDevice.gatt) {
            console.log('[BLE] Found previously paired device:', pairedDevice.name);
            console.log('[BLE] Attempting to connect to previously paired device...');
            shouldTryPairedDevice = true;
            
            try {
              // Try to connect to the paired device
              const gattServer = await pairedDevice.gatt.connect();
              console.log('[BLE] Successfully connected to previously paired device');
              bleDevice = pairedDevice;
              
              // Set up disconnection event listener
              bleDevice.addEventListener('gattserverdisconnected', async () => {
                console.log('[BLE] Device disconnected');
                setIsConnected(false);
                setCharacteristics({});
                await attemptReconnection(bleDevice);
              });
              
              // Continue with service discovery
              await discoverServicesAndCharacteristics(gattServer, bleDevice);
              return; // Success - exit early
              
            } catch (connectErr) {
              console.warn('[BLE] Failed to connect to previously paired device:', connectErr.message);
              console.log('[BLE] Will show device picker instead...');
              // Fall through to show picker
            }
          }
        } catch (err) {
          console.warn('[BLE] getDevices() failed or not supported:', err.message);
        }
      }
      
      // If no previously paired device found or connection failed, request new pairing
      console.log('[BLE] Requesting device pairing...');
      bleDevice = await navigator.bluetooth.requestDevice({
        filters: [
          { name: 'ESP32C3_Timer' },
          { services: [SERVICE_UUID] }
        ],
        optionalServices: [SERVICE_UUID, BATTERY_SERVICE_UUID]
      });
      console.log('[BLE] Device selected:', bleDevice.name);

      // Set up disconnection event listener with automatic reconnection
      bleDevice.addEventListener('gattserverdisconnected', async () => {
        console.log('[BLE] Device disconnected');
        setIsConnected(false);
        setCharacteristics({});
        
        // Attempt automatic reconnection with exponential backoff
        await attemptReconnection(bleDevice);
      });

      // Connect to GATT server
      console.log('[BLE] Connecting to GATT server...');
      const gattServer = await bleDevice.gatt.connect();
      console.log('[BLE] Connected to GATT server');
      
      // Discover services and characteristics
      await discoverServicesAndCharacteristics(gattServer, bleDevice);

    } catch (err) {
      console.error('[BLE] Connection error:', err);
      setError(err.message);
      setIsConnecting(false);
      setIsConnected(false);
    }
  }, [attemptReconnection]);

  // Helper function to discover services and characteristics
  const discoverServicesAndCharacteristics = async (gattServer, bleDevice) => {
    // Discover service 0x00FF
    console.log('[BLE] Discovering service 0x00FF...');
    const service = await gattServer.getPrimaryService(SERVICE_UUID);
    console.log('[BLE] Service discovered');

    // Discover Battery Service
    let batteryService = null;
    try {
      console.log('[BLE] Discovering Battery Service 0x180F...');
      batteryService = await gattServer.getPrimaryService(BATTERY_SERVICE_UUID);
      console.log('[BLE] Battery Service discovered');
    } catch (err) {
      console.warn('[BLE] Battery Service not available:', err.message);
    }

    // Discover all characteristics
    console.log('[BLE] Discovering characteristics...');
    const chars = {};
    
    // Helper function to safely get characteristic
    const getChar = async (uuid, name) => {
      try {
        const char = await service.getCharacteristic(uuid);
        console.log(`[BLE] Found ${name} (${uuid.toString(16)})`);
        return char;
      } catch (err) {
        console.warn(`[BLE] ${name} (${uuid.toString(16)}) not available:`, err.message);
        return null;
      }
    };

    // Try to get each characteristic (some may be optional)
    chars[CHAR_TEMPERATURE] = await getChar(CHAR_TEMPERATURE, 'Temperature');
    chars[CHAR_CURRENT_TIME] = await getChar(CHAR_CURRENT_TIME, 'Current Time');
    chars[CHAR_RELAY_STATE] = await getChar(CHAR_RELAY_STATE, 'Relay State');
    chars[CHAR_SCHEDULE] = await getChar(CHAR_SCHEDULE, 'Schedule');
    chars[CHAR_TEMP_THRESHOLDS] = await getChar(CHAR_TEMP_THRESHOLDS, 'Temperature Thresholds');
    chars[CHAR_TEMP_CALIBRATION] = await getChar(CHAR_TEMP_CALIBRATION, 'Temperature Calibration');
    chars[CHAR_WIFI_SSID] = await getChar(CHAR_WIFI_SSID, 'WiFi SSID');
    chars[CHAR_WIFI_PASSWORD] = await getChar(CHAR_WIFI_PASSWORD, 'WiFi Password');
    chars[CHAR_BLE_PASSKEY] = await getChar(CHAR_BLE_PASSKEY, 'BLE Passkey');

    // Get Battery Level from Battery Service if available
    if (batteryService) {
      try {
        const batteryChar = await batteryService.getCharacteristic(CHAR_BATTERY_LEVEL);
        chars[CHAR_BATTERY_LEVEL] = batteryChar;
        console.log('[BLE] Found Battery Level (0x2A19)');
      } catch (err) {
        console.warn('[BLE] Battery Level (0x2A19) not available:', err.message);
      }
    }

    // Filter out null characteristics
    const availableChars = Object.fromEntries(
      Object.entries(chars).filter(([_, char]) => char !== null)
    );

    console.log(`[BLE] Discovered ${Object.keys(availableChars).length} characteristics`);

    setDevice(bleDevice);
    setServer(gattServer);
    setCharacteristics(chars);
    setIsConnected(true);
    setIsConnecting(false);
    setReconnectAttempts(0); // Reset reconnect attempts on successful connection

    console.log('[BLE] Connection complete');
  };

  const disconnect = useCallback(async () => {
    if (device && device.gatt.connected) {
      console.log('[BLE] Disconnecting...');
      device.gatt.disconnect();
      setIsConnected(false);
      setCharacteristics({});
      setOperationQueue([]); // Clear queue on explicit disconnect
      setReconnectAttempts(0);
      console.log('[BLE] Disconnected');
    }
  }, [device]);

  // Read operations with DataFormatter
  const readTemperature = useCallback(async () => {
    if (!isConnected || !characteristics[CHAR_TEMPERATURE]) {
      throw new Error('Not connected or characteristic not available');
    }

    try {
      console.log('[BLE] Reading temperature...');
      const value = await characteristics[CHAR_TEMPERATURE].readValue();
      const temperature = DataFormatter.decodeTemperature(value);
      console.log('[BLE] Temperature:', temperature, '°C');
      
      setDeviceData(prev => ({ ...prev, temperature }));
      return temperature;
    } catch (err) {
      console.error('[BLE] Failed to read temperature:', err);
      throw err;
    }
  }, [isConnected, characteristics]);

  const readCurrentTime = useCallback(async () => {
    if (!isConnected || !characteristics[CHAR_CURRENT_TIME]) {
      throw new Error('Not connected or characteristic not available');
    }

    try {
      console.log('[BLE] Reading current time...');
      const value = await characteristics[CHAR_CURRENT_TIME].readValue();
      const currentTime = DataFormatter.decodeCurrentTime(value);
      console.log('[BLE] Current time:', currentTime);
      
      setDeviceData(prev => ({ ...prev, currentTime }));
      return currentTime;
    } catch (err) {
      console.error('[BLE] Failed to read current time:', err);
      throw err;
    }
  }, [isConnected, characteristics]);

  const readRelayState = useCallback(async () => {
    if (!isConnected || !characteristics[CHAR_RELAY_STATE]) {
      throw new Error('Not connected or characteristic not available');
    }

    try {
      console.log('[BLE] Reading relay state...');
      const value = await characteristics[CHAR_RELAY_STATE].readValue();
      const { state, overrideEndTime } = DataFormatter.decodeRelayState(value);
      console.log('[BLE] Relay state:', state ? 'ON' : 'OFF', 
                  overrideEndTime ? `(override until ${overrideEndTime})` : '(no override)');
      
      setDeviceData(prev => ({ 
        ...prev, 
        relayState: state,
        relayOverrideEndTime: overrideEndTime
      }));
      return { state, overrideEndTime };
    } catch (err) {
      console.error('[BLE] Failed to read relay state:', err);
      throw err;
    }
  }, [isConnected, characteristics]);

  const readSchedule = useCallback(async () => {
    if (!isConnected || !characteristics[CHAR_SCHEDULE]) {
      throw new Error('Not connected or characteristic not available');
    }

    try {
      console.log('[BLE] Reading schedule...');
      
      // Step 1: Write 0xFF to request full schedule and reset offset
      const requestCmd = new Uint8Array([0xFF]);
      await characteristics[CHAR_SCHEDULE].writeValue(requestCmd);
      console.log('[BLE] Sent schedule read request (0xFF)');
      
      // Small delay to let device prepare
      await new Promise(resolve => setTimeout(resolve, 50));
      
      // Step 2: Read chunks until we get an empty response
      let scheduleBuffer = '';
      const maxChunkSize = 22;
      let chunkCount = 0;
      
      while (true) {
        const value = await characteristics[CHAR_SCHEDULE].readValue();
        
        if (value.byteLength === 0) {
          console.log('[BLE] Received empty chunk, read complete');
          break;
        }
        
        const decoder = new TextDecoder('utf-8');
        const chunk = decoder.decode(value);
        scheduleBuffer += chunk;
        chunkCount++;
        
        console.log(`[BLE] Received chunk ${chunkCount}: ${chunk.length} bytes, total: ${scheduleBuffer.length}`);
        
        // If we received less than max chunk size, we're done
        if (value.byteLength < maxChunkSize) {
          console.log('[BLE] Received final chunk (partial), read complete');
          break;
        }
        
        // Safety limit
        if (chunkCount > 20) {
          console.warn('[BLE] Safety limit reached, stopping read');
          break;
        }
        
        // Small delay between reads
        await new Promise(resolve => setTimeout(resolve, 20));
      }
      
      console.log(`[BLE] Schedule read complete: ${scheduleBuffer.length} bytes in ${chunkCount} chunks`);
      const schedule = DataFormatter.decodeSchedule(scheduleBuffer);
      console.log('[BLE] Schedule:', schedule);
      
      setDeviceData(prev => ({ ...prev, schedule }));
      return schedule;
    } catch (err) {
      console.error('[BLE] Failed to read schedule:', err);
      throw err;
    }
  }, [isConnected, characteristics]);

  const readTemperatureThresholds = useCallback(async () => {
    if (!isConnected || !characteristics[CHAR_TEMP_THRESHOLDS]) {
      throw new Error('Not connected or characteristic not available');
    }

    try {
      console.log('[BLE] Reading temperature thresholds...');
      const value = await characteristics[CHAR_TEMP_THRESHOLDS].readValue();
      const thresholds = DataFormatter.decodeTemperatureThresholds(value);
      console.log('[BLE] Thresholds:', thresholds);
      
      setDeviceData(prev => ({ ...prev, thresholds }));
      return thresholds;
    } catch (err) {
      console.error('[BLE] Failed to read temperature thresholds:', err);
      throw err;
    }
  }, [isConnected, characteristics]);

  const readBatteryLevel = useCallback(async () => {
    if (!isConnected || !characteristics[CHAR_BATTERY_LEVEL]) {
      throw new Error('Not connected or characteristic not available');
    }

    try {
      console.log('[BLE] Reading battery level...');
      const value = await characteristics[CHAR_BATTERY_LEVEL].readValue();
      const batteryLevel = value.getUint8(0);
      console.log('[BLE] Battery level:', batteryLevel, '%');
      
      setDeviceData(prev => ({ ...prev, batteryLevel }));
      return batteryLevel;
    } catch (err) {
      console.error('[BLE] Failed to read battery level:', err);
      throw err;
    }
  }, [isConnected, characteristics]);

  // Write operations with DataFormatter
  const writeRelayState = useCallback(async (state, durationMinutes = 60) => {
    if (!isConnected || !characteristics[CHAR_RELAY_STATE]) {
      throw new Error('Not connected or characteristic not available');
    }

    try {
      console.log('[BLE] Writing relay state:', state ? 'ON' : 'OFF', `for ${durationMinutes} minutes`);
      const data = DataFormatter.encodeRelayState(state, durationMinutes);
      await characteristics[CHAR_RELAY_STATE].writeValue(data);
      console.log('[BLE] Relay state written successfully');
      
      setDeviceData(prev => ({ ...prev, relayState: state }));
      
      // Re-read to get the override end time
      setTimeout(() => readRelayState(), 100);
      
      return true;
    } catch (err) {
      console.error('[BLE] Failed to write relay state:', err);
      throw err;
    }
  }, [isConnected, characteristics, readRelayState]);

  const writeSchedule = useCallback(async (day, scheduleString) => {
    if (!isConnected || !characteristics[CHAR_SCHEDULE]) {
      throw new Error('Not connected or characteristic not available');
    }

    if (!DataFormatter.validateDay(day)) {
      throw new Error('Invalid day index (must be 0-6)');
    }

    if (!DataFormatter.validateScheduleString(scheduleString)) {
      throw new Error('Invalid schedule string (must be 24 characters of H/L/O)');
    }

    try {
      console.log('[BLE] Writing schedule for day', day, ':', scheduleString);
      const data = DataFormatter.encodeSchedule(day, scheduleString);
      await characteristics[CHAR_SCHEDULE].writeValue(data);
      console.log('[BLE] Schedule written successfully');
      
      // Update local schedule state
      setDeviceData(prev => {
        const newSchedule = prev.schedule ? [...prev.schedule] : Array(7).fill('OOOOOOOOOOOOOOOOOOOOOOOO');
        newSchedule[day] = scheduleString;
        return { ...prev, schedule: newSchedule };
      });
      return true;
    } catch (err) {
      console.error('[BLE] Failed to write schedule:', err);
      throw err;
    }
  }, [isConnected, characteristics]);

  const writeTemperatureThresholds = useCallback(async (high, low) => {
    if (!isConnected || !characteristics[CHAR_TEMP_THRESHOLDS]) {
      throw new Error('Not connected or characteristic not available');
    }

    if (!DataFormatter.validateTemperatureThresholds(high, low)) {
      throw new Error('Invalid temperature thresholds (high must be > low)');
    }

    try {
      console.log('[BLE] Writing temperature thresholds:', { high, low });
      const data = DataFormatter.encodeTemperatureThresholds(high, low);
      await characteristics[CHAR_TEMP_THRESHOLDS].writeValue(data);
      console.log('[BLE] Temperature thresholds written successfully');
      
      setDeviceData(prev => ({ ...prev, thresholds: { high, low } }));
      return true;
    } catch (err) {
      console.error('[BLE] Failed to write temperature thresholds:', err);
      throw err;
    }
  }, [isConnected, characteristics]);

  const writeTemperatureCalibration = useCallback(async (temp) => {
    if (!isConnected || !characteristics[CHAR_TEMP_CALIBRATION]) {
      throw new Error('Not connected or characteristic not available');
    }

    try {
      console.log('[BLE] Writing temperature calibration:', temp);
      const data = DataFormatter.encodeTemperatureCalibration(temp);
      await characteristics[CHAR_TEMP_CALIBRATION].writeValue(data);
      console.log('[BLE] Temperature calibration written successfully');
      return true;
    } catch (err) {
      console.error('[BLE] Failed to write temperature calibration:', err);
      throw err;
    }
  }, [isConnected, characteristics]);

  // WiFi and BLE configuration operations
  const readWifiSsid = useCallback(async () => {
    if (!isConnected || !characteristics[CHAR_WIFI_SSID]) {
      throw new Error('Not connected or characteristic not available');
    }

    try {
      console.log('[BLE] Reading WiFi SSID...');
      const value = await characteristics[CHAR_WIFI_SSID].readValue();
      const decoder = new TextDecoder('utf-8');
      const ssid = decoder.decode(value);
      console.log('[BLE] WiFi SSID:', ssid);
      
      setDeviceData(prev => ({ ...prev, wifiSsid: ssid }));
      return ssid;
    } catch (err) {
      console.error('[BLE] Failed to read WiFi SSID:', err);
      throw err;
    }
  }, [isConnected, characteristics]);

  const writeWifiSsid = useCallback(async (ssid) => {
    if (!isConnected || !characteristics[CHAR_WIFI_SSID]) {
      throw new Error('Not connected or characteristic not available');
    }

    if (!ssid || ssid.length === 0 || ssid.length > 32) {
      throw new Error('Invalid SSID length (must be 1-32 characters)');
    }

    try {
      console.log('[BLE] Writing WiFi SSID:', ssid);
      const encoder = new TextEncoder();
      const data = encoder.encode(ssid);
      await characteristics[CHAR_WIFI_SSID].writeValue(data);
      console.log('[BLE] WiFi SSID written successfully');
      
      setDeviceData(prev => ({ ...prev, wifiSsid: ssid }));
      return true;
    } catch (err) {
      console.error('[BLE] Failed to write WiFi SSID:', err);
      throw err;
    }
  }, [isConnected, characteristics]);

  const writeWifiPassword = useCallback(async (password) => {
    if (!isConnected || !characteristics[CHAR_WIFI_PASSWORD]) {
      throw new Error('Not connected or characteristic not available');
    }

    if (!password || password.length === 0 || password.length > 64) {
      throw new Error('Invalid password length (must be 1-64 characters)');
    }

    try {
      console.log('[BLE] Writing WiFi Password...');
      const encoder = new TextEncoder();
      const data = encoder.encode(password);
      await characteristics[CHAR_WIFI_PASSWORD].writeValue(data);
      console.log('[BLE] WiFi Password written successfully');
      return true;
    } catch (err) {
      console.error('[BLE] Failed to write WiFi Password:', err);
      throw err;
    }
  }, [isConnected, characteristics]);

  const writeBlePasskey = useCallback(async (passkey) => {
    if (!isConnected || !characteristics[CHAR_BLE_PASSKEY]) {
      throw new Error('Not connected or characteristic not available');
    }

    if (!/^\d{6}$/.test(passkey)) {
      throw new Error('Invalid passkey format (must be exactly 6 digits)');
    }

    try {
      console.log('[BLE] Writing BLE Passkey...');
      const encoder = new TextEncoder();
      const data = encoder.encode(passkey);
      await characteristics[CHAR_BLE_PASSKEY].writeValue(data);
      console.log('[BLE] BLE Passkey written successfully');
      return true;
    } catch (err) {
      console.error('[BLE] Failed to write BLE Passkey:', err);
      throw err;
    }
  }, [isConnected, characteristics]);

  // Notification setup and handling
  const enableNotifications = useCallback(async (characteristicUuid, handler) => {
    if (!isConnected || !characteristics[characteristicUuid]) {
      throw new Error('Not connected or characteristic not available');
    }

    try {
      const char = characteristics[characteristicUuid];
      console.log(`[BLE] Enabling notifications for characteristic ${characteristicUuid.toString(16)}...`);
      
      // Add event listener for notifications
      char.addEventListener('characteristicvaluechanged', handler);
      
      // Start notifications
      await char.startNotifications();
      console.log(`[BLE] Notifications enabled for ${characteristicUuid.toString(16)}`);
      
      return true;
    } catch (err) {
      console.error(`[BLE] Failed to enable notifications for ${characteristicUuid.toString(16)}:`, err);
      throw err;
    }
  }, [isConnected, characteristics]);

  const setupNotifications = useCallback(async () => {
    if (!isConnected) {
      return;
    }

    try {
      // Set up temperature notifications
      if (characteristics[CHAR_TEMPERATURE]) {
        const tempHandler = (event) => {
          const value = event.target.value;
          const temperature = DataFormatter.decodeTemperature(value);
          console.log('[BLE] Temperature notification:', temperature, '°C');
          setDeviceData(prev => ({ ...prev, temperature }));
        };
        await enableNotifications(CHAR_TEMPERATURE, tempHandler);
      }

      // Set up current time notifications
      if (characteristics[CHAR_CURRENT_TIME]) {
        const timeHandler = (event) => {
          const value = event.target.value;
          const currentTime = DataFormatter.decodeCurrentTime(value);
          console.log('[BLE] Current time notification:', currentTime);
          setDeviceData(prev => ({ ...prev, currentTime }));
        };
        await enableNotifications(CHAR_CURRENT_TIME, timeHandler);
      }

      // Set up battery level notifications
      if (characteristics[CHAR_BATTERY_LEVEL]) {
        const batteryHandler = (event) => {
          const value = event.target.value;
          const batteryLevel = value.getUint8(0);
          console.log('[BLE] Battery level notification:', batteryLevel, '%');
          setDeviceData(prev => ({ ...prev, batteryLevel }));
        };
        await enableNotifications(CHAR_BATTERY_LEVEL, batteryHandler);
      }

      console.log('[BLE] All notifications set up successfully');
    } catch (err) {
      console.error('[BLE] Failed to set up notifications:', err);
      // Don't throw - notifications are optional
    }
  }, [isConnected, characteristics, enableNotifications]);

  // Operation queuing logic
  const MAX_QUEUE_SIZE = 10;

  const queueOperation = useCallback((operation) => {
    setOperationQueue(prev => {
      // Limit queue size to 10 operations
      if (prev.length >= MAX_QUEUE_SIZE) {
        console.warn('[BLE] Operation queue full, dropping oldest operation');
        return [...prev.slice(1), operation];
      }
      console.log('[BLE] Queuing operation:', operation.type, operation.characteristic?.toString(16));
      return [...prev, operation];
    });
  }, []);

  const executeQueuedOperations = useCallback(async () => {
    if (operationQueue.length === 0) {
      return;
    }

    console.log(`[BLE] Executing ${operationQueue.length} queued operations...`);
    
    // Execute operations in FIFO order
    for (const operation of operationQueue) {
      try {
        console.log('[BLE] Executing queued operation:', operation.type, operation.characteristic?.toString(16));
        
        if (operation.type === 'read') {
          const value = await characteristics[operation.characteristic]?.readValue();
          if (operation.resolve) {
            operation.resolve(value);
          }
        } else if (operation.type === 'write') {
          await characteristics[operation.characteristic]?.writeValue(operation.value);
          if (operation.resolve) {
            operation.resolve(true);
          }
        }
      } catch (err) {
        console.error('[BLE] Failed to execute queued operation:', err);
        if (operation.reject) {
          operation.reject(err);
        }
      }
    }

    // Clear queue after execution
    setOperationQueue([]);
    console.log('[BLE] Queued operations executed and queue cleared');
  }, [operationQueue, characteristics]);

  // Execute queued operations when connection is re-established
  useEffect(() => {
    if (isConnected && operationQueue.length > 0) {
      console.log('[BLE] Connection re-established, executing queued operations...');
      executeQueuedOperations();
    }
  }, [isConnected, operationQueue.length, executeQueuedOperations]);

  return {
    isConnected,
    isConnecting,
    error,
    device,
    server,
    characteristics,
    deviceData,
    operationQueue,
    reconnectAttempts,
    connect,
    disconnect,
    readTemperature,
    readCurrentTime,
    readRelayState,
    readSchedule,
    readTemperatureThresholds,
    readBatteryLevel,
    readWifiSsid,
    writeRelayState,
    writeSchedule,
    writeTemperatureThresholds,
    writeTemperatureCalibration,
    writeWifiSsid,
    writeWifiPassword,
    writeBlePasskey,
    setupNotifications,
    queueOperation,
    executeQueuedOperations
  };
};
