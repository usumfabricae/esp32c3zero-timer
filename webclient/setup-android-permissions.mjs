#!/usr/bin/env node

/**
 * Script to inject Bluetooth permissions into AndroidManifest.xml
 * This is needed because Capacitor doesn't automatically add all required permissions
 * for Android 12+ (API 31+) Bluetooth functionality
 */

import { readFileSync, writeFileSync } from 'fs';
import { join } from 'path';

const manifestPath = join(process.cwd(), 'android', 'app', 'src', 'main', 'AndroidManifest.xml');

console.log('Adding Bluetooth permissions to AndroidManifest.xml...');

try {
  let manifest = readFileSync(manifestPath, 'utf8');
  
  // Check if permissions are already added
  if (manifest.includes('android.permission.BLUETOOTH_SCAN')) {
    console.log('Bluetooth permissions already present, skipping...');
    process.exit(0);
  }
  
  // Define the permissions to add
  const permissions = `
    <!-- Bluetooth permissions for Android 12+ (API 31+) -->
    <uses-permission android:name="android.permission.BLUETOOTH_SCAN"
        android:usesPermissionFlags="neverForLocation" />
    <uses-permission android:name="android.permission.BLUETOOTH_CONNECT" />
    
    <!-- Legacy Bluetooth permissions for Android 11 and below -->
    <uses-permission android:name="android.permission.BLUETOOTH" 
        android:maxSdkVersion="30" />
    <uses-permission android:name="android.permission.BLUETOOTH_ADMIN" 
        android:maxSdkVersion="30" />
    
    <!-- Location permission required for BLE plugin initialization -->
    <!-- Note: On Android 12+, BLUETOOTH_SCAN with neverForLocation flag means this won't actually be used for location -->
    <uses-permission android:name="android.permission.ACCESS_FINE_LOCATION" />
    
    <!-- Bluetooth feature requirement -->
    <uses-feature android:name="android.hardware.bluetooth_le" android:required="true" />
`;
  
  // Find the closing </manifest> tag and insert permissions before it
  manifest = manifest.replace('</manifest>', `${permissions}\n</manifest>`);
  
  // Write the modified manifest back
  writeFileSync(manifestPath, manifest, 'utf8');
  
  console.log('Ô£ô Bluetooth permissions added successfully');
  process.exit(0);
  
} catch (error) {
  console.error('Error adding Bluetooth permissions:', error.message);
  process.exit(1);
}
