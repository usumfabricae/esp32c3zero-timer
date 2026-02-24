import { useState, useCallback, useEffect, useRef } from 'react';
import { BlePeripheral, BleProperty, BlePermission } from '@superfede/capacitor-ble-peripheral';

// BLE Service and Characteristic UUIDs (matching ESP32 implementation)
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

export const useBLEPeripheral = () => {
  const [isAdvertising, setIsAdvertising] = useState(false);
  const [isConnected, setIsConnected] = useState(false);
  const [error, setError] = useState(null);
  const [connectedDevice, setConnectedDevice] = useState(null);
  
  // Device state
  const [deviceState, setDeviceState] = useState({
    temperature: 20.0,
    currentTime: new Date(),
    relayState: 0, // 0=Off, 1=Low, 2=High
    relayOverrideEndTime: null,
    schedule: Array(7).fill(null).map(() => Array(24).fill('O')),
    thresholds: { high: 22.0, low: 18.0 },
    batteryLevel: 100,
    batteryVoltage: 4.2,
    wifiSsid: ''
  });

  const listenersRef = useRef([]);

  // Initialize BLE peripheral
  const initialize = useCallback(async () => {
    try {
      await BlePeripheral.initialize();
      console.log('[BLE Peripheral] Initialized');
      
      // Setup services and characteristics
      await setupServices();
      
      return true;
    } catch (err) {
      console.error('[BLE Peripheral] Initialization failed:', err);
      setError(err.message || 'Initialization failed');
      return false;
    }
  }, []);

  // Setup GATT services and characteristics
  const setupServices = useCallback(async () => {
    try {
      // Add main service
      await BlePeripheral.addService({
        uuid: SERVICE_UUID,
        primary: true
      });

      // Add battery service
      await BlePeripheral.addService({
        uuid: BATTERY_SERVICE_UUID,
        primary: true
      });

      // Temperature characteristic (READ, NOTIFY)
      await BlePeripheral.addCharacteristic({
        serviceUuid: SERVICE_UUID,
        uuid: CHAR_TEMPERATURE,
        properties: BleProperty.READ | BleProperty.NOTIFY,
        permissions: BlePermission.READ,
        value: encodeTemperature(deviceState.temperature)
      });

      // Current time characteristic (READ, NOTIFY)
      await BlePeripheral.addCharacteristic({
        serviceUuid: SERVICE_UUID,
        uuid: CHAR_CURRENT_TIME,
        properties: BleProperty.READ | BleProperty.NOTIFY,
        permissions: BlePermission.READ,
        value: encodeCurrentTime(deviceState.currentTime)
      });

      // Relay state characteristic (READ, WRITE)
      await BlePeripheral.addCharacteristic({
        serviceUuid: SERVICE_UUID,
        uuid: CHAR_RELAY_STATE,
        properties: BleProperty.READ | BleProperty.WRITE,
        permissions: BlePermission.READ | BlePermission.WRITE,
        value: encodeRelayState(deviceState.relayState, deviceState.relayOverrideEndTime)
      });

      // Schedule characteristic (READ, WRITE)
      await BlePeripheral.addCharacteristic({
        serviceUuid: SERVICE_UUID,
        uuid: CHAR_SCHEDULE,
        properties: BleProperty.READ | BleProperty.WRITE,
        permissions: BlePermission.READ | BlePermission.WRITE,
        value: btoa('')
      });

      // Temperature thresholds characteristic (READ, WRITE)
      await BlePeripheral.addCharacteristic({
        serviceUuid: SERVICE_UUID,
        uuid: CHAR_TEMP_THRESHOLDS,
        properties: BleProperty.READ | BleProperty.WRITE,
        permissions: BlePermission.READ | BlePermission.WRITE,
        value: encodeTemperatureThresholds(deviceState.thresholds.high, deviceState.thresholds.low)
      });

      // Temperature calibration characteristic (WRITE)
      await BlePeripheral.addCharacteristic({
        serviceUuid: SERVICE_UUID,
        uuid: CHAR_TEMP_CALIBRATION,
        properties: BleProperty.WRITE,
        permissions: BlePermission.WRITE,
        value: btoa('')
      });

      // WiFi SSID characteristic (READ, WRITE)
      await BlePeripheral.addCharacteristic({
        serviceUuid: SERVICE_UUID,
        uuid: CHAR_WIFI_SSID,
        properties: BleProperty.READ | BleProperty.WRITE,
        permissions: BlePermission.READ | BlePermission.WRITE,
        value: btoa(deviceState.wifiSsid)
      });

      // WiFi password characteristic (WRITE)
      await BlePeripheral.addCharacteristic({
        serviceUuid: SERVICE_UUID,
        uuid: CHAR_WIFI_PASSWORD,
        properties: BleProperty.WRITE,
        permissions: BlePermission.WRITE,
        value: btoa('')
      });

      // BLE passkey characteristic (WRITE)
      await BlePeripheral.addCharacteristic({
        serviceUuid: SERVICE_UUID,
        uuid: CHAR_BLE_PASSKEY,
        properties: BleProperty.WRITE,
        permissions: BlePermission.WRITE,
        value: btoa('')
      });

      // Battery level characteristic (READ, NOTIFY)
      await BlePeripheral.addCharacteristic({
        serviceUuid: BATTERY_SERVICE_UUID,
        uuid: CHAR_BATTERY_LEVEL,
        properties: BleProperty.READ | BleProperty.NOTIFY,
        permissions: BlePermission.READ,
        value: encodeBatteryLevel(deviceState.batteryLevel)
      });

      console.log('[BLE Peripheral] Services and characteristics configured');
    } catch (err) {
      console.error('[BLE Peripheral] Setup services failed:', err);
      throw err;
    }
  }, [deviceState]);

  // Start advertising
  const startAdvertising = useCallback(async (deviceName = 'ESP32C3_Timer') => {
    try {
      await BlePeripheral.startAdvertising({
        name: deviceName,
        serviceUuids: [SERVICE_UUID, BATTERY_SERVICE_UUID],
        connectable: true,
        timeout: 0
      });

      setIsAdvertising(true);
      setError(null);
      console.log('[BLE Peripheral] Started advertising as:', deviceName);
    } catch (err) {
      console.error('[BLE Peripheral] Start advertising failed:', err);
      setError(err.message || 'Failed to start advertising');
      throw err;
    }
  }, []);

  // Stop advertising
  const stopAdvertising = useCallback(async () => {
    try {
      await BlePeripheral.stopAdvertising();
      setIsAdvertising(false);
      console.log('[BLE Peripheral] Stopped advertising');
    } catch (err) {
      console.error('[BLE Peripheral] Stop advertising failed:', err);
      setError(err.message || 'Failed to stop advertising');
    }
  }, []);

  // Update temperature and notify
  const updateTemperature = useCallback(async (temperature) => {
    try {
      setDeviceState(prev => ({ ...prev, temperature }));
      
      const value = encodeTemperature(temperature);
      await BlePeripheral.updateCharacteristicValue({
        serviceUuid: SERVICE_UUID,
        characteristicUuid: CHAR_TEMPERATURE,
        value
      });

      if (isConnected) {
        await BlePeripheral.sendNotification({
          serviceUuid: SERVICE_UUID,
          characteristicUuid: CHAR_TEMPERATURE,
          value
        });
      }
    } catch (err) {
      console.error('[BLE Peripheral] Update temperature failed:', err);
    }
  }, [isConnected]);

  // Update current time and notify
  const updateCurrentTime = useCallback(async (time) => {
    try {
      setDeviceState(prev => ({ ...prev, currentTime: time }));
      
      const value = encodeCurrentTime(time);
      await BlePeripheral.updateCharacteristicValue({
        serviceUuid: SERVICE_UUID,
        characteristicUuid: CHAR_CURRENT_TIME,
        value
      });

      if (isConnected) {
        await BlePeripheral.sendNotification({
          serviceUuid: SERVICE_UUID,
          characteristicUuid: CHAR_CURRENT_TIME,
          value
        });
      }
    } catch (err) {
      console.error('[BLE Peripheral] Update time failed:', err);
    }
  }, [isConnected]);

  // Update battery level and notify
  const updateBatteryLevel = useCallback(async (level) => {
    try {
      setDeviceState(prev => ({ ...prev, batteryLevel: level }));
      
      const value = encodeBatteryLevel(level);
      await BlePeripheral.updateCharacteristicValue({
        serviceUuid: BATTERY_SERVICE_UUID,
        characteristicUuid: CHAR_BATTERY_LEVEL,
        value
      });

      if (isConnected) {
        await BlePeripheral.sendNotification({
          serviceUuid: BATTERY_SERVICE_UUID,
          characteristicUuid: CHAR_BATTERY_LEVEL,
          value
        });
      }
    } catch (err) {
      console.error('[BLE Peripheral] Update battery failed:', err);
    }
  }, [isConnected]);

  // Setup event listeners
  useEffect(() => {
    const setupListeners = async () => {
      // Connection state listener
      const connListener = await BlePeripheral.addListener('connectionStateChange', (data) => {
        console.log('[BLE Peripheral] Connection state:', data);
        setIsConnected(data.isConnected);
        setConnectedDevice(data.deviceAddress || null);
      });

      // Characteristic write listener
      const writeListener = await BlePeripheral.addListener('characteristicWrite', (data) => {
        console.log('[BLE Peripheral] Write received:', data);
        handleCharacteristicWrite(data);
      });

      // Characteristic read listener
      const readListener = await BlePeripheral.addListener('characteristicRead', (data) => {
        console.log('[BLE Peripheral] Read request:', data);
      });

      listenersRef.current = [connListener, writeListener, readListener];
    };

    setupListeners();

    return () => {
      // Cleanup listeners
      listenersRef.current.forEach(listener => {
        if (listener && listener.remove) {
          listener.remove();
        }
      });
    };
  }, []);

  // Handle characteristic write requests
  const handleCharacteristicWrite = useCallback((data) => {
    const { characteristicUuid, value } = data;
    const decodedValue = atob(value);

    try {
      switch (characteristicUuid) {
        case CHAR_RELAY_STATE:
          handleRelayStateWrite(decodedValue);
          break;
        case CHAR_SCHEDULE:
          handleScheduleWrite(decodedValue);
          break;
        case CHAR_TEMP_THRESHOLDS:
          handleThresholdsWrite(decodedValue);
          break;
        case CHAR_WIFI_SSID:
          setDeviceState(prev => ({ ...prev, wifiSsid: decodedValue }));
          break;
        default:
          console.log('[BLE Peripheral] Unhandled write to:', characteristicUuid);
      }
    } catch (err) {
      console.error('[BLE Peripheral] Handle write failed:', err);
    }
  }, []);

  // Encoding functions (matching ESP32 format)
  const encodeTemperature = (temp) => {
    const buffer = new ArrayBuffer(2);
    const view = new DataView(buffer);
    view.setInt16(0, Math.round(temp * 100), true);
    return btoa(String.fromCharCode(...new Uint8Array(buffer)));
  };

  const encodeCurrentTime = (date) => {
    const buffer = new ArrayBuffer(10);
    const view = new DataView(buffer);
    view.setUint16(0, date.getFullYear(), true);
    view.setUint8(2, date.getMonth() + 1);
    view.setUint8(3, date.getDate());
    view.setUint8(4, date.getHours());
    view.setUint8(5, date.getMinutes());
    view.setUint8(6, date.getSeconds());
    view.setUint8(7, date.getDay());
    return btoa(String.fromCharCode(...new Uint8Array(buffer)));
  };

  const encodeRelayState = (state, overrideEndTime) => {
    const buffer = new ArrayBuffer(5);
    const view = new DataView(buffer);
    view.setUint8(0, state);
    if (overrideEndTime) {
      view.setUint32(1, Math.floor(overrideEndTime.getTime() / 1000), true);
    }
    return btoa(String.fromCharCode(...new Uint8Array(buffer)));
  };

  const encodeTemperatureThresholds = (high, low) => {
    const buffer = new ArrayBuffer(4);
    const view = new DataView(buffer);
    view.setInt16(0, Math.round(high * 100), true);
    view.setInt16(2, Math.round(low * 100), true);
    return btoa(String.fromCharCode(...new Uint8Array(buffer)));
  };

  const encodeBatteryLevel = (level) => {
    const buffer = new ArrayBuffer(1);
    const view = new DataView(buffer);
    view.setUint8(0, Math.min(100, Math.max(0, level)));
    return btoa(String.fromCharCode(...new Uint8Array(buffer)));
  };

  const handleRelayStateWrite = (value) => {
    // Decode relay state write
    const bytes = new Uint8Array(value.split('').map(c => c.charCodeAt(0)));
    const state = bytes[0];
    setDeviceState(prev => ({ ...prev, relayState: state }));
    console.log('[BLE Peripheral] Relay state updated:', state);
  };

  const handleScheduleWrite = (value) => {
    // Decode schedule write
    console.log('[BLE Peripheral] Schedule write:', value);
    // Implementation depends on schedule format
  };

  const handleThresholdsWrite = (value) => {
    // Decode thresholds write
    const bytes = new Uint8Array(value.split('').map(c => c.charCodeAt(0)));
    const view = new DataView(bytes.buffer);
    const high = view.getInt16(0, true) / 100;
    const low = view.getInt16(2, true) / 100;
    setDeviceState(prev => ({ ...prev, thresholds: { high, low } }));
    console.log('[BLE Peripheral] Thresholds updated:', { high, low });
  };

  return {
    isAdvertising,
    isConnected,
    connectedDevice,
    error,
    deviceState,
    initialize,
    startAdvertising,
    stopAdvertising,
    updateTemperature,
    updateCurrentTime,
    updateBatteryLevel,
    setDeviceState
  };
};
