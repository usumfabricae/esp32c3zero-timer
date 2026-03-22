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

  const clientRef = useRef(null);
  const pollIntervalRef = useRef(null);
  const debounceTimerRef = useRef(null);
  const pendingUpdateRef = useRef(null);
  const thingNameRef = useRef(null);
  // Local pending desired state: tracks what we've written (or seen in desired)
  // until reported confirms the values. This survives polls and shadow clears.
  const localDesiredRef = useRef({});

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
   * Check if a local desired value has been confirmed by reported state.
   * Returns true if reported matches the desired value (device applied it).
   */
  const isConfirmed = useCallback((reportedVal, desiredVal) => {
    return JSON.stringify(reportedVal) === JSON.stringify(desiredVal);
  }, []);

  /**
   * Merge local desired state with shadow desired state.
   * Shadow desired may have been set by another app instance.
   * Local desired tracks our own writes. We keep whichever is newer
   * (local wins for keys we wrote, shadow wins for keys we didn't).
   */
  const mergeDesired = useCallback((shadowDesired) => {
    if (!shadowDesired) return localDesiredRef.current;
    const merged = { ...localDesiredRef.current };

    // Absorb shadow desired keys we don't have locally (from other apps)
    if (shadowDesired.thresholds?.high !== undefined && shadowDesired.thresholds?.low !== undefined
        && !shadowDesired.thresholds.command && !merged.thresholds) {
      merged.thresholds = { high: shadowDesired.thresholds.high, low: shadowDesired.thresholds.low };
    }
    if (shadowDesired.override?.active !== undefined && !shadowDesired.override.command && !merged.override) {
      merged.override = shadowDesired.override;
    }
    if (shadowDesired.schedule && !shadowDesired.schedule.command) {
      // Merge schedule day-by-day: local wins per-day, shadow fills gaps
      merged.schedule = { ...(shadowDesired.schedule || {}), ...(merged.schedule || {}) };
    }

    return merged;
  }, []);

  /**
   * Parse shadow state into deviceData format.
   *
   * Strategy: for every configurable field, the effective value shown in the UI
   * is determined by this priority chain:
   *   1. localDesired (our own writes, not yet confirmed by reported)
   *   2. shadow desired (another app's writes, not yet confirmed)
   *   3. reported (device's confirmed state)
   *
   * A localDesired entry is cleared once reported confirms it (values match).
   * This means the UI never reverts to stale reported data while a command is
   * pending, regardless of how many polls happen or whether AWS clears desired.
   */
  const parseShadowState = useCallback((reported, desired) => {
    if (!reported) return;

    const data = {};
    const effectiveDesired = mergeDesired(desired);

    // --- Read-only telemetry: always from reported ---
    if (reported.temperature !== undefined)
      data.temperature = reported.temperature;
    if (reported.battery !== undefined)
      data.batteryLevel = reported.battery;
    if (reported.timestamp)
      data.currentTime = new Date(reported.timestamp * 1000).toLocaleString();

    // --- Thresholds ---
    const desiredThresholds = effectiveDesired.thresholds;
    const reportedThresholds = reported.thresholds;
    if (desiredThresholds?.high !== undefined && desiredThresholds?.low !== undefined) {
      // Check if reported has caught up
      if (reportedThresholds && isConfirmed(
        { high: reportedThresholds.high, low: reportedThresholds.low },
        { high: desiredThresholds.high, low: desiredThresholds.low }
      )) {
        // Confirmed — clear local desired for thresholds
        delete localDesiredRef.current.thresholds;
        data.thresholds = { high: reportedThresholds.high, low: reportedThresholds.low };
      } else {
        // Still pending — show desired values
        data.thresholds = { high: desiredThresholds.high, low: desiredThresholds.low };
      }
    } else if (reportedThresholds) {
      data.thresholds = { high: reportedThresholds.high, low: reportedThresholds.low };
    }

    // --- Override / relay state ---
    const desiredOverride = effectiveDesired.override;
    if (desiredOverride?.active !== undefined) {
      if (reported.relay_state !== undefined) {
        const reportedBool = reported.relay_state === 'H' || reported.relay_state === 'L';
        if (reportedBool === desiredOverride.active) {
          delete localDesiredRef.current.override;
          data.relayState = reportedBool;
        } else {
          data.relayState = desiredOverride.active;
        }
      } else {
        data.relayState = desiredOverride.active;
      }
    } else if (reported.relay_state !== undefined) {
      data.relayState = reported.relay_state === 'H' || reported.relay_state === 'L';
    }

    // --- Schedule ---
    const desiredSchedule = effectiveDesired.schedule;
    const reportedSchedule = reported.schedule || {};
    if (desiredSchedule && Object.keys(desiredSchedule).length > 0) {
      const dayNames = ['monday', 'tuesday', 'wednesday', 'thursday', 'friday', 'saturday', 'sunday'];
      const mergedSchedule = { ...reportedSchedule };
      let allConfirmed = true;

      for (const day of dayNames) {
        if (desiredSchedule[day] !== undefined) {
          if (reportedSchedule[day] === desiredSchedule[day]) {
            // This day is confirmed
          } else {
            allConfirmed = false;
            mergedSchedule[day] = desiredSchedule[day];
          }
        }
      }

      if (allConfirmed) {
        delete localDesiredRef.current.schedule;
      }

      data.schedule = convertScheduleToArray(mergedSchedule);
    } else if (Object.keys(reportedSchedule).length > 0) {
      data.schedule = convertScheduleToArray(reportedSchedule);
    }

    if (Object.keys(data).length > 0) {
      setDeviceData(prev => ({ ...prev, ...data }));
    }
  }, [convertScheduleToArray, mergeDesired, isConfirmed]);

  /**
   * Detect pending commands by checking local desired state and shadow desired.
   * Commands are pending if localDesiredRef has any keys (not yet confirmed)
   * or if shadow desired differs from reported.
   */
  const checkPendingCommands = useCallback((shadow) => {
    // Local desired entries that haven't been confirmed yet
    const hasLocalPending = Object.keys(localDesiredRef.current).length > 0;
    if (hasLocalPending) {
      setHasPendingCommands(true);
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
  }, []);

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
      setHasPendingCommands(true);
    } catch (err) {
      console.error('[IoT] Failed to update shadow:', err);
      throw err;
    }
  }, []);

  /**
   * Update device shadow desired state (debounced to max 1/second).
   * Deep-merges 'schedule' key since days are sent individually.
   */
  const updateDeviceShadow = useCallback((desiredState) => {
    // Deep-merge: for 'schedule', merge day keys instead of replacing
    const current = pendingUpdateRef.current || {};
    const merged = { ...current, ...desiredState };
    if (current.schedule && desiredState.schedule) {
      merged.schedule = { ...current.schedule, ...desiredState.schedule };
    }
    pendingUpdateRef.current = merged;

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
    localDesiredRef.current = {};
    setIsConnected(false);
    setHasPendingCommands(false);
    setLastSyncTime(null);
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
    // Store in local desired so polls won't overwrite until reported confirms
    localDesiredRef.current.override = { active: !!state, duration_minutes: durationMinutes };
    updateDeviceShadow({
      override: { active: !!state, duration_minutes: durationMinutes }
    });
    setDeviceData(prev => ({ ...prev, relayState: state }));
  }, [updateDeviceShadow]);

  const writeSchedule = useCallback(async (day, scheduleString) => {
    const dayNames = ['monday', 'tuesday', 'wednesday', 'thursday', 'friday', 'saturday', 'sunday'];
    const dayName = dayNames[day];
    if (!dayName) return;

    // Merge into local desired schedule (day-by-day)
    localDesiredRef.current.schedule = {
      ...(localDesiredRef.current.schedule || {}),
      [dayName]: scheduleString
    };
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
    // Store in local desired so polls won't overwrite until reported confirms
    localDesiredRef.current.thresholds = { high, low };
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
