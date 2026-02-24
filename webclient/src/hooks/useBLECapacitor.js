import { useState, useCallback, useEffect } from 'react';
import { Capacitor } from '@capacitor/core';
import { BleClient } from '@capacitor-community/bluetooth-le';
import DataFormatter from '../utils/dataFormatter.js';

// BLE Service and Characteristic UUIDs
const SERVICE_UUID = '000000ff-0000-1000-8000-00805f9b34fb';
const BATTERY_SERVICE_UUID = '0000180f-0000-1000-8000-00805f9b34fb';
const CHAR_TEMPERATURE = '00002a6e-0000-1000-8000-00805f9b34fb';
const CHAR_CURRENT_TIME = '00002a2b-0000-1000-8000-00805f9b34fb';
const CHAR_RELAY_STATE = '0000ff02-0000-1000-8000-00805f9b34fb';
const CHAR_SCHEDULE = '0000ff05-0000-1000-8000-00805f9b34fb';
const CHAR_TEMP_THRESHOLDS = '0000ff06-0000-1000-8000-00805f9b34fb';
const CHAR_TEMP_CALIBRATION = '0000ff07-0000-1000-8000-00805f9b34fb';
const CHAR_WIFI_SSID = '0000ff08-0000-1000-8000-00805f9b34fb';
const CHAR_WIFI_PASSWORD = '0000ff09-0000-1000-8000-00805f9b34fb';
const CHAR_BLE_PASSKEY = '0000ff0a-0000-1000-8000-00805f9b34fb';
const CHAR_BATTERY_LEVEL = '00002a19-0000-1000-8000-00805f9b34fb';

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
      if (deviceId) {
        BleClient.disconnect(deviceId).catch(console.error);
      }
    };
  }, []);

  // Connect to device
  const connect = useCallback(async () => {
    setIsConnecting(true);
    setError(null);

    try {
      console.log('[Capacitor BLE] Requesting device...');
      
      // Request device with filters
      const device = await BleClient.requestDevice({
        services: [SERVICE_UUID],
        optionalServices: [BATTERY_SERVICE_UUID],
        name: 'ESP32C3_Timer'
      });

      console.log('[Capacitor BLE] Device found:', device.deviceId);
      
      // Connect to device
      await BleClient.connect(device.deviceId, (disconnectedDeviceId) => {
        console.log('[Capacitor BLE] Device disconnected:', disconnectedDeviceId);
        setIsConnected(false);
        setDeviceId(null);
      });

      console.log('[Capacitor BLE] Connected to device');
      setDeviceId(device.deviceId);
      setIsConnected(true);
      setIsConnecting(false);

    } catch (err) {
      console.error('[Capacitor BLE] Connection failed:', err);
      setError(err.message || 'Connection failed');
      setIsConnecting(false);
      setIsConnected(false);
    }
  }, []);

  // Disconnect from device
  const disconnect = useCallback(async () => {
    if (!deviceId) return;

    try {
      await BleClient.disconnect(deviceId);
      setIsConnected(false);
      setDeviceId(null);
      console.log('[Capacitor BLE] Disconnected');
    } catch (err) {
      console.error('[Capacitor BLE] Disconnect failed:', err);
      setError(err.message || 'Disconnect failed');
    }
  }, [deviceId]);

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
    try {
      const data = await readCharacteristic(CHAR_TEMPERATURE);
      const temp = DataFormatter.parseTemperature(data);
      setDeviceData(prev => ({ ...prev, temperature: temp }));
      return temp;
    } catch (err) {
      console.error('[Capacitor BLE] Read temperature failed:', err);
      throw err;
    }
  }, [readCharacteristic]);

  // Read current time
  const readCurrentTime = useCallback(async () => {
    try {
      const data = await readCharacteristic(CHAR_CURRENT_TIME);
      const time = DataFormatter.parseCurrentTime(data);
      setDeviceData(prev => ({ ...prev, currentTime: time }));
      return time;
    } catch (err) {
      console.error('[Capacitor BLE] Read current time failed:', err);
      throw err;
    }
  }, [readCharacteristic]);

  // Read relay state
  const readRelayState = useCallback(async () => {
    try {
      const data = await readCharacteristic(CHAR_RELAY_STATE);
      const { state, overrideEndTime } = DataFormatter.parseRelayState(data);
      setDeviceData(prev => ({ 
        ...prev, 
        relayState: state,
        relayOverrideEndTime: overrideEndTime 
      }));
      return { state, overrideEndTime };
    } catch (err) {
      console.error('[Capacitor BLE] Read relay state failed:', err);
      throw err;
    }
  }, [readCharacteristic]);

  // Write relay state
  const writeRelayState = useCallback(async (state, durationMinutes) => {
    try {
      const data = DataFormatter.formatRelayState(state, durationMinutes);
      await writeCharacteristic(CHAR_RELAY_STATE, data);
      await readRelayState(); // Read back to update state
    } catch (err) {
      console.error('[Capacitor BLE] Write relay state failed:', err);
      throw err;
    }
  }, [writeCharacteristic, readRelayState]);

  // Read schedule
  const readSchedule = useCallback(async () => {
    try {
      const scheduleData = {};
      const days = ['monday', 'tuesday', 'wednesday', 'thursday', 'friday', 'saturday', 'sunday'];
      
      for (let i = 0; i < 7; i++) {
        const data = await readCharacteristic(CHAR_SCHEDULE);
        const daySchedule = DataFormatter.parseSchedule(data);
        scheduleData[days[i]] = daySchedule;
      }
      
      setDeviceData(prev => ({ ...prev, schedule: scheduleData }));
      return scheduleData;
    } catch (err) {
      console.error('[Capacitor BLE] Read schedule failed:', err);
      throw err;
    }
  }, [readCharacteristic]);

  // Write schedule
  const writeSchedule = useCallback(async (day, schedule) => {
    try {
      const data = DataFormatter.formatSchedule(day, schedule);
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
      const thresholds = DataFormatter.parseTemperatureThresholds(data);
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
      const data = DataFormatter.formatTemperatureThresholds(highTemp, lowTemp);
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
      const { level, voltage } = DataFormatter.parseBatteryLevel(data);
      setDeviceData(prev => ({ 
        ...prev, 
        batteryLevel: level,
        batteryVoltage: voltage 
      }));
      return { level, voltage };
    } catch (err) {
      console.error('[Capacitor BLE] Read battery failed:', err);
      throw err;
    }
  }, [deviceId]);

  // Setup notifications
  const setupNotifications = useCallback(async () => {
    if (!deviceId || !isConnected) return;

    try {
      // Temperature notifications
      await BleClient.startNotifications(
        deviceId,
        SERVICE_UUID,
        CHAR_TEMPERATURE,
        (data) => {
          const temp = DataFormatter.parseTemperature(data);
          setDeviceData(prev => ({ ...prev, temperature: temp }));
        }
      );

      // Time notifications
      await BleClient.startNotifications(
        deviceId,
        SERVICE_UUID,
        CHAR_CURRENT_TIME,
        (data) => {
          const time = DataFormatter.parseCurrentTime(data);
          setDeviceData(prev => ({ ...prev, currentTime: time }));
        }
      );

      // Battery notifications
      await BleClient.startNotifications(
        deviceId,
        BATTERY_SERVICE_UUID,
        CHAR_BATTERY_LEVEL,
        (data) => {
          const { level, voltage } = DataFormatter.parseBatteryLevel(data);
          setDeviceData(prev => ({ 
            ...prev, 
            batteryLevel: level,
            batteryVoltage: voltage 
          }));
        }
      );

      console.log('[Capacitor BLE] Notifications enabled');
    } catch (err) {
      console.error('[Capacitor BLE] Setup notifications failed:', err);
    }
  }, [deviceId, isConnected]);

  return {
    isConnected,
    isConnecting,
    error,
    deviceData,
    connect,
    disconnect,
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
    reconnectAttempts: 0
  };
};
