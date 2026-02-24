#!/usr/bin/env node

/**
 * Script to add Bluetooth permissions to AndroidManifest.xml
 * Run after: npx cap add android
 */

const fs = require('fs');
const path = require('path');

const manifestPath = path.join(__dirname, 'android', 'app', 'src', 'main', 'AndroidManifest.xml');

// Check if android folder exists
if (!fs.existsSync(path.join(__dirname, 'android'))) {
  console.log('❌ Android folder not found. Run "npx cap add android" first.');
  process.exit(1);
}

// Check if manifest exists
if (!fs.existsSync(manifestPath)) {
  console.log('❌ AndroidManifest.xml not found at:', manifestPath);
  process.exit(1);
}

// Read manifest
let manifest = fs.readFileSync(manifestPath, 'utf8');

// Permissions to add
const permissions = [
  '    <!-- Bluetooth permissions for Android 12+ -->',
  '    <uses-permission android:name="android.permission.BLUETOOTH_SCAN" android:usesPermissionFlags="neverForLocation" />',
  '    <uses-permission android:name="android.permission.BLUETOOTH_CONNECT" />',
  '    ',
  '    <!-- Bluetooth permissions for Android 11 and below -->',
  '    <uses-permission android:name="android.permission.BLUETOOTH" android:maxSdkVersion="30" />',
  '    <uses-permission android:name="android.permission.BLUETOOTH_ADMIN" android:maxSdkVersion="30" />',
  '    <uses-permission android:name="android.permission.ACCESS_FINE_LOCATION" android:maxSdkVersion="30" />'
];

// Check if permissions already exist
if (manifest.includes('BLUETOOTH_SCAN')) {
  console.log('✅ Bluetooth permissions already present in AndroidManifest.xml');
  process.exit(0);
}

// Find the manifest tag and add permissions after it
const manifestTagRegex = /<manifest[^>]*>/;
const match = manifest.match(manifestTagRegex);

if (!match) {
  console.log('❌ Could not find <manifest> tag in AndroidManifest.xml');
  process.exit(1);
}

// Insert permissions after manifest tag
const insertPosition = match.index + match[0].length;
const permissionsBlock = '\n' + permissions.join('\n') + '\n';
manifest = manifest.slice(0, insertPosition) + permissionsBlock + manifest.slice(insertPosition);

// Write back to file
fs.writeFileSync(manifestPath, manifest, 'utf8');

console.log('✅ Bluetooth permissions added to AndroidManifest.xml');
console.log('');
console.log('Added permissions:');
permissions.forEach(p => console.log(p));
