import { useState, useCallback, useRef, useEffect } from 'react';
import { IoTDataPlaneClient, GetThingShadowCommand, UpdateThingShadowCommand } from '@aws-sdk/client-iot-data-plane';

const IOT_ENDPOINT = 'azjenkdbqqlx7-ats.iot.eu-central-1.amazonaws.com';
const SHADOW_POLL_INTERVAL = 5000; // 5 seconds
const DEBOUNCE_INTERVAL = 1000; // 1 second max update rate

/**
 * useIoT hook for AWS IoT Core Device Shadow communication.
 * Reads reported state, writes desired state, polls for updates.
 */
export const useIoT = () => {
  const [isConnected, setIsConnected] = useState(false);
  const [isConnecting, setIsConnecting] = useState(false);
  const [error, setError] = useState(null);
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
  const [lastSyncTime, setLastSyncTime] = useState(null);
  const [hasPendingCommands, setHasPendingCommands] = useState(false);
  const [sessionHasCommands, setSessionHasCommands] = useState(false);

  const clientRef = useRef(null);
  const pollIntervalRef = useRef(null);
  const debounceTimerRef = useRef(null);
  const pendingUpdateRef = useRef(null);
  const thingNameRef = useRef(null);

  /**
   * Convert shadow schedule object (day names → strings) to array format
   * Firmware publishes: { monday: "HHHLLL...", tuesday: "...", ... }
   * Dashboard expects: ["HHHLLL...", "HHHLLL...", ...] (index 0=Mon, 6=Sun)
   */
  const convertScheduleToArray = useCallback((scheduleObj) => {
    if (!scheduleObj) return null;
    // If already an array, return as-is
    if (Array.isArray(scheduleObj)) return scheduleObj;
    const dayNames = ['monday', 'tuesday', 'wednesday', 'thursday', 'friday', 'saturday', 'sunday'];
    const arr = dayNames.map(day => scheduleObj[day] || 'OOOOOOOOOOOOOOOOOOOOOOOO');
    return arr;
  }, []);

  /**
   * Track which fields have pending desired state to avoid overwriting optimistic updates
   */
  const pendingFieldsRef = useRef(new Set());

  /**
   * Parse shadow reported state into deviceData format.
   * Skips fields that have pending desired state to avoid reverting optimistic updates.
   */
  const parseShadowState = useCallback((reported, desired) => {
    if (!reported) return;

    const pending = pendingFieldsRef.current;
    const data = {};

    if (reported.temperature !== undefined && !pending.has('temperature'))
      data.temperature = reported.temperature;
    if (reported.battery !== undefined && !pending.has('battery'))
      data.batteryLevel = reported.battery;
    if (reported.relay_state !== undefined && !pending.has('relayState'))
      data.relayState = reported.relay_state === 'H' || reported.relay_state === 'L';
    if (reported.thresholds && !pending.has('thresholds'))
      data.thresholds = { high: reported.thresholds.high, low: reported.thresholds.low };
    if (reported.schedule && !pending.has('schedule'))
      data.schedule = convertScheduleToArray(reported.schedule);
    if (reported.timestamp)
      data.currentTime = new Date(reported.timestamp * 1000).toLocaleString();

    // Clear pending fields when reported matches desired (device applied the change)
    if (desired && pending.size > 0) {
      if (pending.has('thresholds') && reported.thresholds && desired.thresholds &&
          reported.thresholds.high === desired.thresholds.high &&
          reported.thresholds.low === desired.thresholds.low) {
        pending.delete('thresholds');
        data.thresholds = { high: reported.thresholds.high, low: reported.thresholds.low };
      }
      if (pending.has('relayState') && desired.override &&
          reported.override && JSON.stringify(reported.override) === JSON.stringify(desired.override)) {
        pending.delete('relayState');
      }
      if (pending.has('schedule') && reported.schedule && desired.schedule) {
        const reportedKeys = Object.keys(desired.schedule).every(
          k => reported.schedule[k] === desired.schedule[k]
        );
        if (reportedKeys) {
          pending.delete('schedule');
          data.schedule = convertScheduleToArray(reported.schedule);
        }
      }
    }

    if (Object.keys(data).length > 0) {
      setDeviceData(prev => ({ ...prev, ...data }));
    }
  }, [convertScheduleToArray]);

  /**
   * Detect pending commands by comparing reported vs desired
   */
  const checkPendingCommands = useCallback((shadow) => {
    if (!sessionHasCommands) {
      setHasPendingCommands(false);
      return;
    }
    const reported = shadow?.state?.reported;
    const desired = shadow?.state?.desired;
    if (!desired || Object.keys(desired).length === 0) {
      setHasPendingCommands(false);
      return;
    }
    const pending = Object.keys(desired).some(key => {
      return JSON.stringify(desired[key]) !== JSON.stringify(reported?.[key]);
    });
    setHasPendingCommands(pending);
  }, [sessionHasCommands]);

  /**
   * Read device shadow
   */
  const getDeviceShadow = useCallback(async () => {
    if (!clientRef.current || !thingNameRef.current) return null;

    try {
      const command = new GetThingShadowCommand({ thingName: thingNameRef.current });
      const response = await clientRef.current.send(command);
      const payload = JSON.parse(new TextDecoder().decode(response.payload));

      // Extract last sync time from metadata
      const metaTimestamp = payload?.metadata?.reported?.timestamp;
      if (metaTimestamp) {
        // Shadow metadata timestamps are Unix epoch seconds
        setLastSyncTime(new Date(metaTimestamp * 1000));
      }

      parseShadowState(payload?.state?.reported, payload?.state?.desired);
      checkPendingCommands(payload);

      return payload;
    } catch (err) {
      console.error('[IoT] Failed to get shadow:', err);
      throw err;
    }
  }, [parseShadowState, checkPendingCommands]);

  /**
   * Flush pending debounced update
   */
  const flushUpdate = useCallback(async () => {
    if (!pendingUpdateRef.current || !clientRef.current || !thingNameRef.current) return;

    const desiredState = pendingUpdateRef.current;
    pendingUpdateRef.current = null;

    try {
      const payload = JSON.stringify({ state: { desired: desiredState } });
      const command = new UpdateThingShadowCommand({
        thingName: thingNameRef.current,
        payload: new TextEncoder().encode(payload)
      });
      await clientRef.current.send(command);
      console.log('[IoT] Shadow desired state updated');
      setSessionHasCommands(true);
      setHasPendingCommands(true);
    } catch (err) {
      console.error('[IoT] Failed to update shadow:', err);
      throw err;
    }
  }, []);

  /**
   * Update device shadow desired state (debounced to max 1/second)
   */
  const updateDeviceShadow = useCallback((desiredState) => {
    // Merge with any pending update
    pendingUpdateRef.current = { ...(pendingUpdateRef.current || {}), ...desiredState };

    if (debounceTimerRef.current) return; // Already scheduled

    debounceTimerRef.current = setTimeout(() => {
      debounceTimerRef.current = null;
      flushUpdate();
    }, DEBOUNCE_INTERVAL);
  }, [flushUpdate]);

  /**
   * Connect to AWS IoT Core
   */
  const connect = useCallback(async (config = {}) => {
    const { thingName, region, credentials } = config;

    if (!thingName) {
      throw new Error('Thing name is required for IoT connection');
    }

    setIsConnecting(true);
    setError(null);

    try {
      const endpoint = config.endpoint || IOT_ENDPOINT;
      thingNameRef.current = thingName;

      const clientConfig = {
        endpoint: `https://${endpoint}`,
        region: region || 'eu-central-1'
      };

      // Use provided credentials or fall back to environment
      if (credentials?.accessKeyId && credentials?.secretAccessKey) {
        clientConfig.credentials = {
          accessKeyId: credentials.accessKeyId,
          secretAccessKey: credentials.secretAccessKey,
          ...(credentials.sessionToken && { sessionToken: credentials.sessionToken })
        };
      }

      clientRef.current = new IoTDataPlaneClient(clientConfig);

      // Test connection by reading shadow
      await getDeviceShadow();

      setIsConnected(true);
      setIsConnecting(false);

      // Start polling for shadow updates
      pollIntervalRef.current = setInterval(() => {
        getDeviceShadow().catch(err => {
          console.error('[IoT] Shadow poll failed:', err);
        });
      }, SHADOW_POLL_INTERVAL);

      console.log('[IoT] Connected to IoT Core');
    } catch (err) {
      console.error('[IoT] Connection failed:', err);
      setError(err.message);
      setIsConnecting(false);
      setIsConnected(false);
      clientRef.current = null;
      thingNameRef.current = null;
      throw err;
    }
  }, [getDeviceShadow]);

  /**
   * Disconnect from AWS IoT Core
   */
  const disconnect = useCallback(async () => {
    if (pollIntervalRef.current) {
      clearInterval(pollIntervalRef.current);
      pollIntervalRef.current = null;
    }
    if (debounceTimerRef.current) {
      clearTimeout(debounceTimerRef.current);
      debounceTimerRef.current = null;
    }
    clientRef.current = null;
    thingNameRef.current = null;
    setIsConnected(false);
    setHasPendingCommands(false);
    setSessionHasCommands(false);
    setLastSyncTime(null);
    pendingFieldsRef.current.clear();
    console.log('[IoT] Disconnected');
  }, []);

  // Cleanup on unmount
  useEffect(() => {
    return () => {
      if (pollIntervalRef.current) clearInterval(pollIntervalRef.current);
      if (debounceTimerRef.current) clearTimeout(debounceTimerRef.current);
    };
  }, []);

  // --- BLE-compatible write operations via shadow desired state ---

  const writeRelayState = useCallback(async (state, durationMinutes = 60) => {
    pendingFieldsRef.current.add('relayState');
    updateDeviceShadow({
      override: { active: !!state, duration_minutes: durationMinutes }
    });
    setDeviceData(prev => ({ ...prev, relayState: state }));
  }, [updateDeviceShadow]);

  const writeSchedule = useCallback(async (day, scheduleString) => {
    const dayNames = ['monday', 'tuesday', 'wednesday', 'thursday', 'friday', 'saturday', 'sunday'];
    const dayName = dayNames[day];
    if (!dayName) return;

    pendingFieldsRef.current.add('schedule');
    updateDeviceShadow({
      schedule: { [dayName]: scheduleString }
    });
    setDeviceData(prev => {
      const newSchedule = prev.schedule ? [...prev.schedule] : Array(7).fill('OOOOOOOOOOOOOOOOOOOOOOOO');
      newSchedule[day] = scheduleString;
      return { ...prev, schedule: newSchedule };
    });
  }, [updateDeviceShadow]);

  const writeTemperatureThresholds = useCallback(async (high, low) => {
    pendingFieldsRef.current.add('thresholds');
    updateDeviceShadow({
      thresholds: { high, low }
    });
    setDeviceData(prev => ({ ...prev, thresholds: { high, low } }));
  }, [updateDeviceShadow]);

  const writeTemperatureCalibration = useCallback(async (temp) => {
    updateDeviceShadow({
      calibration: { command: 'calibration', temp_point: temp }
    });
  }, [updateDeviceShadow]);

  const writeBatteryCalibration = useCallback(async (voltageMillivolts) => {
    updateDeviceShadow({
      calibration: { command: 'battery_calibration', battery_point: voltageMillivolts }
    });
  }, [updateDeviceShadow]);

  // No-op stubs for BLE-only operations (not applicable in IoT mode)
  const readTemperature = useCallback(async () => deviceData.temperature, [deviceData.temperature]);
  const readCurrentTime = useCallback(async () => deviceData.currentTime, [deviceData.currentTime]);
  const readRelayState = useCallback(async () => ({ state: deviceData.relayState, overrideEndTime: deviceData.relayOverrideEndTime }), [deviceData.relayState, deviceData.relayOverrideEndTime]);
  const readSchedule = useCallback(async () => deviceData.schedule, [deviceData.schedule]);
  const readTemperatureThresholds = useCallback(async () => deviceData.thresholds, [deviceData.thresholds]);
  const readBatteryLevel = useCallback(async () => deviceData.batteryLevel, [deviceData.batteryLevel]);
  const readWifiSsid = useCallback(async () => deviceData.wifiSsid, [deviceData.wifiSsid]);
  const writeWifiSsid = useCallback(async () => {}, []);
  const writeWifiPassword = useCallback(async () => {}, []);
  const writeBlePasskey = useCallback(async () => {}, []);
  const setupNotifications = useCallback(async () => {}, []);

  return {
    isConnected,
    isConnecting,
    error,
    device: null,
    server: null,
    characteristics: {},
    deviceData,
    operationQueue: [],
    reconnectAttempts: 0,
    lastSyncTime,
    hasPendingCommands,
    connectionMethod: isConnected ? 'iot' : null,
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
    writeBatteryCalibration,
    writeWifiSsid,
    writeWifiPassword,
    writeBlePasskey,
    setupNotifications,
    queueOperation: () => {},
    executeQueuedOperations: () => {},
    getDeviceShadow,
    updateDeviceShadow
  };
};
