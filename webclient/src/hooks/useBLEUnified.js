import { useState, useCallback, useEffect, useRef } from 'react';
import { Capacitor } from '@capacitor/core';
import { useBLE } from './useBLE.js';
import { useBLECapacitor } from './useBLECapacitor.js';
import { useIoT } from './useIoT.js';

const IOT_CONNECTION_TIMEOUT = 5000; // 5 seconds

/**
 * Unified connection hook that tries IoT Core first, then falls back to BLE.
 * Auto-detects native vs web for BLE implementation.
 */
export const useBLEUnified = () => {
  const isNative = Capacitor.isNativePlatform();
  const ble = isNative ? useBLECapacitor() : useBLE();
  const iot = useIoT();

  const [connectionMethod, setConnectionMethod] = useState(null);
  const [iotConfig, setIotConfig] = useState(null);

  // Determine which interface is currently active
  const active = connectionMethod === 'iot' ? iot : ble;

  const isConnected = active.isConnected;
  const isConnecting = iot.isConnecting || ble.isConnecting;
  const error = active.error;
  const deviceData = active.deviceData;

  /**
   * Try IoT Core first with timeout, fall back to BLE
   */
  const connect = useCallback(async (config) => {
    const iotCfg = config?.iot || iotConfig;

    if (iotCfg?.thingName) {
      console.log('[Unified] Attempting IoT Core connection (5s timeout)...');
      try {
        const iotPromise = iot.connect(iotCfg);
        const timeoutPromise = new Promise((_, reject) =>
          setTimeout(() => reject(new Error('IoT connection timeout')), IOT_CONNECTION_TIMEOUT)
        );
        await Promise.race([iotPromise, timeoutPromise]);

        setConnectionMethod('iot');
        console.log('[Unified] Connected via IoT Core');
        return;
      } catch (err) {
        console.warn('[Unified] IoT Core failed, falling back to BLE:', err.message);
        try { await iot.disconnect(); } catch (_) {}
      }
    }

    // Fall back to BLE
    console.log('[Unified] Connecting via BLE...');
    await ble.connect();
    setConnectionMethod('ble');
    console.log('[Unified] Connected via BLE');
  }, [ble, iot, iotConfig]);

  /**
   * Disconnect active interface
   */
  const disconnect = useCallback(async () => {
    if (connectionMethod === 'iot') {
      await iot.disconnect();
    } else {
      await ble.disconnect();
    }
    setConnectionMethod(null);
  }, [connectionMethod, ble, iot]);

  /**
   * Configure IoT credentials for future connection attempts
   */
  const configureIoT = useCallback((config) => {
    setIotConfig(config);
  }, []);

  return {
    // State
    isConnected,
    isConnecting,
    error,
    device: ble.device,
    server: ble.server,
    characteristics: ble.characteristics,
    deviceData,
    operationQueue: ble.operationQueue,
    reconnectAttempts: ble.reconnectAttempts,
    connectionMethod,
    lastSyncTime: iot.lastSyncTime,
    hasPendingCommands: iot.hasPendingCommands,

    // Actions
    connect,
    disconnect,
    configureIoT,

    // Data operations (delegated to active interface)
    readTemperature: active.readTemperature,
    readCurrentTime: active.readCurrentTime,
    readRelayState: active.readRelayState,
    readSchedule: active.readSchedule,
    readTemperatureThresholds: active.readTemperatureThresholds,
    readBatteryLevel: active.readBatteryLevel,
    readWifiSsid: active.readWifiSsid,
    writeRelayState: active.writeRelayState,
    writeSchedule: active.writeSchedule,
    writeTemperatureThresholds: active.writeTemperatureThresholds,
    writeTemperatureCalibration: active.writeTemperatureCalibration,
    writeBatteryCalibration: active.writeBatteryCalibration,
    writeWifiSsid: active.writeWifiSsid,
    writeWifiPassword: active.writeWifiPassword,
    writeBlePasskey: active.writeBlePasskey,
    setupNotifications: active.setupNotifications,
    queueOperation: ble.queueOperation,
    executeQueuedOperations: ble.executeQueuedOperations,

    // IoT-specific
    getDeviceShadow: iot.getDeviceShadow,
    updateDeviceShadow: iot.updateDeviceShadow
  };
};
