import { useState, useCallback, useEffect, useRef } from 'react';
import { BleClient } from '@capacitor-community/bluetooth-le';
import DataFormatter from '../utils/dataFormatter.js';
import { getCurrentTime, getMillisecondsUntilScanTime } from '../utils/ntpSync.js';

// BLE Service and Characteristic UUIDs
const SERVICE_UUID = '000000ff-0000-1000-8000-00805f9b34fb';
const BATTERY_SERVICE_UUID = '0000180f-0000-1000-8000-00805f9b34fb';
const CHAR_TEMPERATURE = '0000ff0c-0000-1000-8000-00805f9b34fb';  // Changed from 0x2A6E to 0xFF0C for Capacitor compatibility
const CHAR_CURRENT_TIME = '00002a2b-0000-1000-8000-00805f9b34fb';
const CHAR_RELAY_STATE = '0000ff02-0000-1000-8000-00805f9b34fb';
const CHAR_SCHEDULE = '0000ff05-0000-1000-8000-00805f9b34fb';
const CHAR_TEMP_THRESHOLDS = '0000ff06-0000-1000-8000-00805f9b34fb';
const CHAR_BATTERY_LEVEL = '00002a19-0000-1000-8000-00805f9b34fb';

// Storage key for saved device
const STORAGE_KEY_DEVICE_ID = 'ble_saved_device_id';

// Convert short UUID to full UUID format
const toFullUUID = (shortUuid) => {
  if (typeof shortUuid === 'number') {
    const hex = shortUuid.toString(16).padStart(4, '0');
    return `0000${hex}-0000-1000-8000-00805f9b34fb`;
  }
  return shortUuid;
};

export const useBLECapacitor = () => {
  const [isConnected, setIsConnected] = useState(false);
  const [isConnecting, setIsConnecting] = useState(false);
  const [error, setError] = useState(null);
  const [deviceId, setDeviceId] = useState(null);
  const [savedDeviceId, setSavedDeviceId] = useState(null);
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

  // Use refs to track scan state across async callbacks
  const scanTimeoutRef = useRef(null);
  const isScanningRef = useRef(false);
  const deviceFoundRef = useRef(false);
  const scheduledScanTimerRef = useRef(null);
  
  // Refs to hold latest notification data without triggering re-renders
  const notificationDataRef = useRef({
    temperature: null,
    currentTime: null,
    batteryLevel: null
  });
  const updateTimerRef = useRef(null);
  
  // Store time source for logging
  const timeSourceRef = useRef('phone');

  // Load saved device ID from localStorage
  useEffect(() => {
    const loadSavedDevice = () => {
      try {
        const saved = localStorage.getItem(STORAGE_KEY_DEVICE_ID);
        if (saved) {
          setSavedDeviceId(saved);
          console.log('[Capacitor BLE] Loaded saved device:', saved);
        }
      } catch (err) {
        console.error('[Capacitor BLE] Failed to load saved device:', err);
      }
    };

    loadSavedDevice();
  }, []);

  // Initialize BLE client on mount
  useEffect(() => {
    const initBLE = async () => {
      try {
        await BleClient.initialize();
        console.log('[Capacitor BLE] Initialized');
      } catch (err) {
        console.error('[Capacitor BLE] Initialization failed:', err);
        setError('Failed to initialize Bluetooth');
      }
    };

    initBLE();

    return () => {
      // Cleanup on unmount
      const cleanup = async () => {
        // Clear update timer
        if (updateTimerRef.current) {
          clearTimeout(updateTimerRef.current);
        }
        
        // Clear scheduled scan timer
        if (scheduledScanTimerRef.current) {
          clearTimeout(scheduledScanTimerRef.current);
        }
        
        // Clear timeout
        if (scanTimeoutRef.current) {
          clearTimeout(scanTimeoutRef.current);
        }
        
        // Stop any ongoing scan
        if (isScanningRef.current) {
          try {
            await BleClient.stopLEScan();
          } catch (err) {
            console.error('[Capacitor BLE] Cleanup scan stop failed:', err);
          }
        }
        
        // Disconnect device
        if (deviceId) {
          try {
            await BleClient.disconnect(deviceId);
          } catch (err) {
            console.error('[Capacitor BLE] Cleanup disconnect failed:', err);
          }
        }
      };
      
      cleanup();
    };
  }, [deviceId]);

  // Background reconnect to saved device using smart scanning
  const reconnectToSavedDevice = useCallback(async (deviceAddress) => {
    setIsConnecting(true);
    setError(null);

    // Reset refs
    deviceFoundRef.current = false;
    isScanningRef.current = false;
    
    // Clear any existing timeout
    if (scanTimeoutRef.current) {
      clearTimeout(scanTimeoutRef.current);
      scanTimeoutRef.current = null;
    }

    const cleanup = async () => {
      console.log('[Capacitor BLE] Cleanup called');
      
      // Clear timeout
      if (scanTimeoutRef.current) {
        clearTimeout(scanTimeoutRef.current);
        scanTimeoutRef.current = null;
      }
      
      // Stop scanning if still active
      if (isScanningRef.current) {
        try {
          await BleClient.stopLEScan();
          isScanningRef.current = false;
          console.log('[Capacitor BLE] Scan stopped');
        } catch (err) {
          console.error('[Capacitor BLE] Failed to stop scan:', err);
        }
      }
    };

    const startScan = async () => {
      try {
        console.log('[Capacitor BLE] Starting scan for saved device:', deviceAddress);
        
        // Start scanning for the specific device with low latency mode
        // SCAN_MODE_LOW_LATENCY (2) provides fastest discovery at cost of battery
        await BleClient.requestLEScan(
          {
            allowDuplicates: false,
            scanMode: 2  // SCAN_MODE_LOW_LATENCY for fastest discovery
          },
          async (result) => {
            // Ignore if device already found or processed
            if (deviceFoundRef.current) {
              return;
            }

            // Check if this is our saved device
            if (result.device.deviceId === deviceAddress) {
              deviceFoundRef.current = true;
              console.log('[Capacitor BLE] Found saved device:', result.device.deviceId);
              
              // Stop scanning immediately
              await cleanup();
              
              // Connect to the device
              try {
                await BleClient.connect(result.device.deviceId, (disconnectedDeviceId) => {
                  console.log('[Capacitor BLE] Device disconnected:', disconnectedDeviceId);
                  setIsConnected(false);
                  setDeviceId(null);
                });
                
                console.log('[Capacitor BLE] Reconnected to saved device');
                setDeviceId(result.device.deviceId);
                setIsConnected(true);
                setIsConnecting(false);
              } catch (err) {
                console.error('[Capacitor BLE] Reconnection failed:', err);
                setError(err.message || 'Reconnection failed');
                setIsConnecting(false);
              }
            }
          }
        );

        isScanningRef.current = true;

        // Stop scanning after 10 seconds if device not found
        // Device should appear within 3-4 seconds if we timed it right
        scanTimeoutRef.current = setTimeout(async () => {
          if (!deviceFoundRef.current) {
            console.log('[Capacitor BLE] Scan timeout - device not found');
            await cleanup();
            
            // Schedule next scan attempt at next minute mark
            // Get fresh time for next calculation
            const { time: currentTime, source } = await getCurrentTime();
            const msUntilNextScan = getMillisecondsUntilScanTime(currentTime);
            console.log(`[Capacitor BLE] Scheduling next scan in ${(msUntilNextScan / 1000).toFixed(1)}s (time source: ${source})`);
            
            scheduledScanTimerRef.current = setTimeout(() => {
              if (!deviceFoundRef.current && !isConnected) {
                startScan();
              }
            }, msUntilNextScan);
          }
        }, 10000);

      } catch (err) {
        console.error('[Capacitor BLE] Scan failed:', err);
        await cleanup();
        setError(err.message || 'Scan failed');
        setIsConnecting(false);
      }
    };

    try {
      // Get accurate time from NTP (with fallback to phone time)
      const { time: currentTime, source } = await getCurrentTime();
      timeSourceRef.current = source;
      
      // Calculate time until 3 seconds before next minute
      const msUntilScan = getMillisecondsUntilScanTime(currentTime);
      console.log(`[Capacitor BLE] Time source: ${source}`);
      console.log(`[Capacitor BLE] Current time: ${currentTime.toISOString()}`);
      console.log(`[Capacitor BLE] Scheduling scan in ${(msUntilScan / 1000).toFixed(1)}s (at XX:XX:57)`);
      
      // Schedule the scan to start at the optimal time
      scheduledScanTimerRef.current = setTimeout(startScan, msUntilScan);

    } catch (err) {
      console.error('[Capacitor BLE] Failed to schedule scan:', err);
      setError(err.message || 'Failed to schedule scan');
      setIsConnecting(false);
    }
  }, [isConnected]);

  // Connect to device (first time or manual)
  const connect = useCallback(async () => {
    // Prevent multiple simultaneous connection attempts
    if (isConnecting) {
      console.log('[Capacitor BLE] Connection already in progress');
      return;
    }

    setIsConnecting(true);
    setError(null);

    try {
      // Get accurate time from NTP before attempting connection
      const { time: currentTime, source } = await getCurrentTime();
      timeSourceRef.current = source;
      console.log(`[Capacitor BLE] Time source: ${source}`);
      console.log(`[Capacitor BLE] Current time: ${currentTime.toISOString()}`);
      
      // If we have a saved device, try to reconnect in background
      if (savedDeviceId) {
        console.log('[Capacitor BLE] Attempting background reconnect...');
        await reconnectToSavedDevice(savedDeviceId);
        return;
      }

      // First time connection - show device picker
      console.log('[Capacitor BLE] Requesting device (first time)...');
      
      // Request device with filters
      const device = await BleClient.requestDevice({
        services: [SERVICE_UUID],
        optionalServices: [BATTERY_SERVICE_UUID],
        name: 'ESP32C3_Timer'
      });

      console.log('[Capacitor BLE] Device found:', device.deviceId);
      
      // Connect to device with timeout
      const connectPromise = BleClient.connect(device.deviceId, (disconnectedDeviceId) => {
        console.log('[Capacitor BLE] Device disconnected:', disconnectedDeviceId);
        setIsConnected(false);
        setDeviceId(null);
      });

      // Add connection timeout (15 seconds)
      const timeoutPromise = new Promise((_, reject) => {
        setTimeout(() => reject(new Error('Connection timeout')), 15000);
      });

      await Promise.race([connectPromise, timeoutPromise]);

      console.log('[Capacitor BLE] Connected to device');
      
      // Save device ID for future connections
      localStorage.setItem(STORAGE_KEY_DEVICE_ID, device.deviceId);
      setSavedDeviceId(device.deviceId);
      
      setDeviceId(device.deviceId);
      setIsConnected(true);
      setIsConnecting(false);

    } catch (err) {
      console.error('[Capacitor BLE] Connection failed:', err);
      setError(err.message || 'Connection failed');
      setIsConnecting(false);
      setIsConnected(false);
      
      // If connection failed, try to disconnect to clean up
      try {
        if (deviceId) {
          await BleClient.disconnect(deviceId);
        }
      } catch (disconnectErr) {
        console.error('[Capacitor BLE] Cleanup disconnect failed:', disconnectErr);
      }
    }
  }, [savedDeviceId, reconnectToSavedDevice, isConnecting, deviceId]);

  // Disconnect from device
  const disconnect = useCallback(async () => {
    if (!deviceId) {
      // Reset state even if no device ID
      setIsConnected(false);
      setIsConnecting(false);
      return;
    }

    try {
      await BleClient.disconnect(deviceId);
      setIsConnected(false);
      setDeviceId(null);
      setIsConnecting(false);
      console.log('[Capacitor BLE] Disconnected');
    } catch (err) {
      console.error('[Capacitor BLE] Disconnect failed:', err);
      // Force reset state even if disconnect fails
      setIsConnected(false);
      setDeviceId(null);
      setIsConnecting(false);
      setError(err.message || 'Disconnect failed');
    }
  }, [deviceId]);

  // Forget saved device (for testing or switching devices)
  const forgetDevice = useCallback(() => {
    try {
      localStorage.removeItem(STORAGE_KEY_DEVICE_ID);
      setSavedDeviceId(null);
      console.log('[Capacitor BLE] Forgot saved device');
    } catch (err) {
      console.error('[Capacitor BLE] Failed to forget device:', err);
    }
  }, []);

  // Read characteristic
  const readCharacteristic = useCallback(async (charUuid) => {
    if (!deviceId || !isConnected) {
      throw new Error('Not connected to device');
    }

    try {
      const fullUuid = toFullUUID(charUuid);
      const result = await BleClient.read(deviceId, SERVICE_UUID, fullUuid);
      return result;
    } catch (err) {
      console.error(`[Capacitor BLE] Read failed for ${charUuid}:`, err);
      throw err;
    }
  }, [deviceId, isConnected]);

  // Write characteristic
  const writeCharacteristic = useCallback(async (charUuid, value) => {
    if (!deviceId || !isConnected) {
      throw new Error('Not connected to device');
    }

    try {
      const fullUuid = toFullUUID(charUuid);
      await BleClient.write(deviceId, SERVICE_UUID, fullUuid, value);
    } catch (err) {
      console.error(`[Capacitor BLE] Write failed for ${charUuid}:`, err);
      throw err;
    }
  }, [deviceId, isConnected]);

  // Read temperature
  const readTemperature = useCallback(async () => {
    console.log('[Capacitor BLE] readTemperature() called');
    try {
      const data = await readCharacteristic(CHAR_TEMPERATURE);
      const temp = DataFormatter.decodeTemperature(data);
      setDeviceData(prev => ({ ...prev, temperature: temp }));
      console.log('[Capacitor BLE] Temperature read successfully:', temp);
      return temp;
    } catch (err) {
      console.error('[Capacitor BLE] Read temperature failed:', err);
      throw err;
    }
  }, [readCharacteristic]);

  // Read current time
  const readCurrentTime = useCallback(async () => {
    console.log('[Capacitor BLE] readCurrentTime() called');
    try {
      const data = await readCharacteristic(CHAR_CURRENT_TIME);
      const time = DataFormatter.decodeCurrentTime(data);
      setDeviceData(prev => ({ ...prev, currentTime: time }));
      console.log('[Capacitor BLE] Current time read successfully:', time);
      return time;
    } catch (err) {
      console.error('[Capacitor BLE] Read current time failed:', err);
      throw err;
    }
  }, [readCharacteristic]);

  // Read relay state
  const readRelayState = useCallback(async () => {
    console.log('[Capacitor BLE] readRelayState() called');
    try {
      const data = await readCharacteristic(CHAR_RELAY_STATE);
      const { state, overrideEndTime } = DataFormatter.decodeRelayState(data);
      setDeviceData(prev => ({ 
        ...prev, 
        relayState: state,
        relayOverrideEndTime: overrideEndTime 
      }));
      console.log('[Capacitor BLE] Relay state read successfully:', state, overrideEndTime);
      return { state, overrideEndTime };
    } catch (err) {
      console.error('[Capacitor BLE] Read relay state failed:', err);
      throw err;
    }
  }, [readCharacteristic]);

  // Write relay state
  const writeRelayState = useCallback(async (state, durationMinutes) => {
    try {
      const data = DataFormatter.encodeRelayState(state, durationMinutes);
      await writeCharacteristic(CHAR_RELAY_STATE, data);
      await readRelayState(); // Read back to update state
    } catch (err) {
      console.error('[Capacitor BLE] Write relay state failed:', err);
      throw err;
    }
  }, [writeCharacteristic, readRelayState]);

  // Read schedule
  const readSchedule = useCallback(async () => {
    console.log('[Capacitor BLE] readSchedule() called');
    try {
      // Request full schedule
      console.log('[Capacitor BLE] Writing 0xFF to schedule characteristic to prepare read');
      const requestCmd = new Uint8Array([0xFF]);
      await writeCharacteristic(CHAR_SCHEDULE, new DataView(requestCmd.buffer));
      
      // Small delay
      await new Promise(resolve => setTimeout(resolve, 50));
      
      // Read chunks
      console.log('[Capacitor BLE] Starting to read schedule chunks');
      let scheduleBuffer = '';
      const maxChunkSize = 22;
      let chunkCount = 0;
      
      while (true) {
        const data = await readCharacteristic(CHAR_SCHEDULE);
        
        if (data.byteLength === 0) {
          console.log('[Capacitor BLE] Schedule read complete (empty chunk received)');
          break;
        }
        
        const decoder = new TextDecoder('utf-8');
        const chunk = decoder.decode(data);
        scheduleBuffer += chunk;
        chunkCount++;
        console.log(`[Capacitor BLE] Read schedule chunk ${chunkCount}: ${data.byteLength} bytes`);
        
        if (data.byteLength < maxChunkSize || chunkCount > 20) {
          console.log('[Capacitor BLE] Schedule read complete (last chunk or max chunks reached)');
          break;
        }
        
        await new Promise(resolve => setTimeout(resolve, 20));
      }
      
      console.log(`[Capacitor BLE] Total schedule data: ${scheduleBuffer.length} chars, ${chunkCount} chunks`);
      const schedule = DataFormatter.decodeSchedule(scheduleBuffer);
      setDeviceData(prev => ({ ...prev, schedule }));
      console.log('[Capacitor BLE] Schedule decoded successfully');
      return schedule;
    } catch (err) {
      console.error('[Capacitor BLE] Read schedule failed:', err);
      throw err;
    }
  }, [readCharacteristic, writeCharacteristic]);

  // Write schedule
  const writeSchedule = useCallback(async (day, schedule) => {
    try {
      const data = DataFormatter.encodeSchedule(day, schedule);
      await writeCharacteristic(CHAR_SCHEDULE, data);
    } catch (err) {
      console.error('[Capacitor BLE] Write schedule failed:', err);
      throw err;
    }
  }, [writeCharacteristic]);

  // Read temperature thresholds
  const readTemperatureThresholds = useCallback(async () => {
    try {
      const data = await readCharacteristic(CHAR_TEMP_THRESHOLDS);
      const thresholds = DataFormatter.decodeTemperatureThresholds(data);
      setDeviceData(prev => ({ ...prev, thresholds }));
      return thresholds;
    } catch (err) {
      console.error('[Capacitor BLE] Read thresholds failed:', err);
      throw err;
    }
  }, [readCharacteristic]);

  // Write temperature thresholds
  const writeTemperatureThresholds = useCallback(async (highTemp, lowTemp) => {
    try {
      const data = DataFormatter.encodeTemperatureThresholds(highTemp, lowTemp);
      await writeCharacteristic(CHAR_TEMP_THRESHOLDS, data);
      await readTemperatureThresholds(); // Read back to update state
    } catch (err) {
      console.error('[Capacitor BLE] Write thresholds failed:', err);
      throw err;
    }
  }, [writeCharacteristic, readTemperatureThresholds]);

  // Read battery level
  const readBatteryLevel = useCallback(async () => {
    try {
      const data = await BleClient.read(deviceId, BATTERY_SERVICE_UUID, CHAR_BATTERY_LEVEL);
      const level = data.getUint8(0);
      setDeviceData(prev => ({ 
        ...prev, 
        batteryLevel: level
      }));
      return level;
    } catch (err) {
      console.error('[Capacitor BLE] Read battery failed:', err);
      throw err;
    }
  }, [deviceId]);

  // Batch update function - updates state at most once per second
  const batchUpdateDeviceData = useCallback(() => {
    setDeviceData(prev => ({
      ...prev,
      ...notificationDataRef.current
    }));
  }, []);

  // Setup notifications (NOT IMPLEMENTED ON DEVICE - this is a no-op for Capacitor)
  const setupNotifications = useCallback(async () => {
    if (!deviceId || !isConnected) return;

    console.log('[Capacitor BLE] setupNotifications() called - skipping (not implemented on device)');
    
    // The ESP32 device declares characteristics with NOTIFY property but doesn't
    // actually send notifications. Attempting to enable notifications causes
    // GATT_READ_NOT_PERMIT errors. The web client should poll values instead.
    
    // Do nothing - just return successfully
    return Promise.resolve();
  }, [deviceId, isConnected]);

  return {
    isConnected,
    isConnecting,
    error,
    deviceData,
    connect,
    disconnect,
    forgetDevice,
    hasSavedDevice: !!savedDeviceId,
    timeSource: timeSourceRef.current,
    readTemperature,
    readCurrentTime,
    readRelayState,
    writeRelayState,
    readSchedule,
    writeSchedule,
    readTemperatureThresholds,
    writeTemperatureThresholds,
    readBatteryLevel,
    setupNotifications,
    // Placeholder functions for compatibility
    readWifiSsid: async () => null,
    writeWifiSsid: async () => {},
    writeWifiPassword: async () => {},
    writeBlePasskey: async () => {},
    writeTemperatureCalibration: async () => {},
    writeBatteryCalibration: async () => {},
    reconnectAttempts: 0
  };
};
