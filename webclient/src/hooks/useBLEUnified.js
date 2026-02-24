import { Capacitor } from '@capacitor/core';
import { useBLE } from './useBLE.js';
import { useBLECapacitor } from './useBLECapacitor.js';

/**
 * Unified BLE hook that automatically detects the environment
 * and uses the appropriate BLE implementation:
 * - Web Bluetooth API for browser/PWA
 * - Capacitor Bluetooth LE for native Android app
 */
export const useBLEUnified = () => {
  const isNative = Capacitor.isNativePlatform();
  
  console.log(`[BLE Unified] Running in ${isNative ? 'native' : 'web'} mode`);
  
  // Use Capacitor BLE for native platforms, Web Bluetooth for web
  if (isNative) {
    return useBLECapacitor();
  } else {
    return useBLE();
  }
};
