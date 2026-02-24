# Codemagic Build Process - Detailed Flow

## Overview
This document traces the exact build process for the BLE Peripheral Demo APK on Codemagic.

---

## Step-by-Step Build Flow

### STEP 1: Install npm dependencies
**Location:** `webclient/`  
**Command:** `npm ci`

**What happens:**
- Installs all dependencies from `package.json`
- Creates `node_modules/` folder
- Installs: React, Vite, Capacitor core, Capacitor Android, Capacitor Bluetooth LE plugin

**Output:**
```
webclient/node_modules/
  ├── react/
  ├── react-dom/
  ├── @capacitor/core/
  ├── @capacitor/android/
  ├── @capacitor-community/bluetooth-le/
  └── ... (other dependencies)
```

---

### STEP 2: Install BLE Peripheral Plugin
**Location:** `webclient/`  
**Command:** `npm install ./ble-peripheral-plugin --no-save`

**What happens:**
- Copies the plugin from `webclient/ble-peripheral-plugin/` to `node_modules/@superfede/capacitor-ble-peripheral/`
- The plugin package.json specifies `"main": "src/index.ts"` (uses source files, not compiled)
- Includes TypeScript definitions and Android Java code

**Plugin Structure (source):**
```
webclient/ble-peripheral-plugin/
  ├── package.json          # Plugin metadata
  ├── src/
  │   ├── index.ts          # Main entry point (exports definitions)
  │   └── definitions.ts    # TypeScript interface definitions
  └── android/
      ├── build.gradle      # Android library build config
      └── src/main/java/net/superfede/capacitor/bleperipheral/
          └── BlePeripheralPlugin.java  # Android implementation
```

**Plugin Structure (installed):**
```
webclient/node_modules/@superfede/capacitor-ble-peripheral/
  ├── package.json          # Copied from source
  ├── src/
  │   ├── index.ts          # TypeScript entry point
  │   └── definitions.ts    # TypeScript definitions
  └── android/
      └── src/main/java/... # Java implementation
```

**Why this plugin exists:**
- The default Capacitor Bluetooth LE plugin only supports CLIENT mode (connecting to BLE devices)
- This custom plugin adds PERIPHERAL/SERVER mode (allowing Android to act as a BLE device)
- Needed for testing: Android phone becomes the BLE server, laptop/another phone connects as client

---

### STEP 3: Verify Plugin Structure
**Location:** `webclient/`  
**Command:** Various `ls` and `cat` commands

**What happens:**
- Verification step to ensure plugin was installed correctly
- Lists files in both source and installed locations
- Displays package.json to confirm configuration

**No build artifacts created** - just verification

---

### STEP 4: Build Main Web App
**Location:** `webclient/`  
**Command:** `CAPACITOR=true npm run build`

**What this does:**
- Runs: `vite build` (uses default `vite.config.js`)
- Builds the MAIN webclient application (the relay timer controller)
- Entry point: `index.html` → `src/main.jsx` → `App.jsx`
- Sets `CAPACITOR=true` environment variable (affects base path in vite config)

**Input files:**
```
webclient/
  ├── index.html                    # Entry HTML
  └── src/
      ├── main.jsx                  # Entry point: renders App.jsx
      ├── App.jsx                   # Main application (relay timer UI)
      ├── components/
      │   ├── Dashboard.jsx         # Timer dashboard
      │   ├── ScheduleGrid.jsx      # Weekly schedule editor
      │   ├── TemperatureSettings.jsx
      │   └── ... (other UI components)
      └── hooks/
          └── useBLEUnified.js      # BLE CLIENT hook (connects TO devices)
```

**Output:**
```
webclient/dist/
  ├── index.html                    # Built HTML (references bundled JS/CSS)
  ├── assets/
  │   ├── index-[hash].js           # Bundled JavaScript (App.jsx + all components)
  │   └── index-[hash].css          # Bundled CSS
  └── manifest.json, icon-192.png, etc.
```

**Why build the main app?**
- Capacitor requires a `dist/` folder to exist before running `npx cap add android`
- The Android platform setup needs valid web assets to initialize properly
- This creates the Android project structure with proper configuration

---

### STEP 5: Setup Android Platform
**Location:** `webclient/`  
**Commands:**
```bash
npx cap add android    # (if android/ doesn't exist)
npx cap sync android
```

**What happens:**

**5a. `npx cap add android` (first time only):**
- Creates `webclient/android/` folder with complete Android project structure
- Copies Capacitor Android runtime
- Generates Gradle build files
- Creates AndroidManifest.xml
- Sets up package name from capacitor.config.json

**5b. `npx cap sync android`:**
- Copies `webclient/dist/` → `webclient/android/app/src/main/assets/public/`
- Scans `node_modules/` for Capacitor plugins
- Finds: `@capacitor-community/bluetooth-le` and `@superfede/capacitor-ble-peripheral`
- Generates plugin registration code
- Updates Android build.gradle to include plugin dependencies
- Creates `capacitor-cordova-android-plugins/` folder for plugin integration

**Android Project Structure Created:**
```
webclient/android/
  ├── app/
  │   ├── src/main/
  │   │   ├── assets/public/          # Web assets copied here
  │   │   │   ├── index.html          # From dist/
  │   │   │   └── assets/
  │   │   │       ├── index-[hash].js # From dist/assets/
  │   │   │       └── index-[hash].css
  │   │   ├── java/net/superfede/BleWebInterface/
  │   │   │   └── MainActivity.java   # Android app entry point
  │   │   └── AndroidManifest.xml
  │   ├── build.gradle                # App-level build config
  │   └── capacitor.build.gradle      # Capacitor plugin integration
  ├── capacitor-cordova-android-plugins/  # Plugin bridge code
  ├── build.gradle                    # Project-level build config
  ├── settings.gradle                 # Gradle settings
  └── gradlew                         # Gradle wrapper script
```

**Plugin Integration:**
- The BLE Peripheral plugin's Android code is linked into the build
- `capacitor.build.gradle` includes: `implementation project(':capacitor-ble-peripheral')`
- Plugin Java code becomes part of the Android app

**Current State:**
- `android/app/src/main/assets/public/` contains the MAIN webclient app
- This is the relay timer controller UI (Dashboard, ScheduleGrid, etc.)

---

### STEP 6: Build Peripheral Demo Web App
**Location:** `webclient/`  
**Command:** `CAPACITOR=true npm run build:peripheral`

**What this does:**
- Runs: `vite build --config vite.config.peripheral.js`
- Builds the PERIPHERAL DEMO application (different from main app)
- Entry point: `index-peripheral.html` → `src/main-peripheral.jsx` → `AppPeripheralDemo.jsx`

**Input files:**
```
webclient/
  ├── index-peripheral.html         # Different entry HTML
  └── src/
      ├── main-peripheral.jsx       # Entry point: renders AppPeripheralDemo.jsx
      ├── AppPeripheralDemo.jsx     # Peripheral demo app (simple test UI)
      ├── components/
      │   └── BLEPeripheralDemo.jsx # Demo component
      └── hooks/
          └── useBLEPeripheral.js   # BLE SERVER hook (acts AS a device)
```

**Key Differences from Main App:**
| Aspect | Main App | Peripheral Demo |
|--------|----------|-----------------|
| Entry HTML | `index.html` | `index-peripheral.html` |
| Entry JS | `src/main.jsx` | `src/main-peripheral.jsx` |
| Root Component | `App.jsx` | `AppPeripheralDemo.jsx` |
| BLE Mode | CLIENT (connects to devices) | SERVER (acts as device) |
| UI | Full relay timer interface | Simple test interface |
| Plugin Used | `@capacitor-community/bluetooth-le` | `@superfede/capacitor-ble-peripheral` |

**Output:**
```
webclient/dist-peripheral/
  ├── index.html                    # Built HTML (references bundled JS/CSS)
  └── assets/
      ├── index-[hash].js           # Bundled JavaScript (AppPeripheralDemo.jsx)
      └── index-[hash].css          # Bundled CSS
```

**Current State:**
- `webclient/dist/` = Main app (relay timer)
- `webclient/dist-peripheral/` = Peripheral demo app
- `android/app/src/main/assets/public/` = Still contains main app

---

### STEP 7: Build Peripheral Demo APK (FIXED VERSION)
**Location:** `webclient/`

**What happens (step by step):**

**7a. Backup current dist:**
```bash
mv dist dist-backup
```
- Renames `dist/` (main app) to `dist-backup/`
- Preserves the main app build

**7b. Swap in peripheral dist:**
```bash
mv dist-peripheral dist
```
- Renames `dist-peripheral/` to `dist/`
- Now `dist/` contains the peripheral demo app

**Current folder state:**
```
webclient/
  ├── dist/                 # ← Peripheral demo (was dist-peripheral)
  ├── dist-backup/          # ← Main app (was dist)
  └── android/
      └── app/src/main/assets/public/  # ← Still has OLD main app
```

**7c. Sync Capacitor:**
```bash
npx cap sync android
```
- Copies `dist/` → `android/app/src/main/assets/public/`
- OVERWRITES the old main app assets with peripheral demo assets
- Updates plugin registrations (no changes, same plugins)

**Current folder state:**
```
webclient/
  ├── dist/                 # ← Peripheral demo
  ├── dist-backup/          # ← Main app
  └── android/
      └── app/src/main/assets/public/  # ← NOW has peripheral demo!
          ├── index.html    # From dist-peripheral (peripheral demo)
          └── assets/
              └── index-[hash].js  # Peripheral demo JavaScript
```

**7d. Build APK:**
```bash
cd android
./gradlew assembleDebug
```
- Gradle builds the Android APK
- Packages everything in `android/app/src/main/assets/public/`
- Creates: `android/app/build/outputs/apk/debug/app-debug.apk`

**APK Contents:**
- Android app wrapper (MainActivity.java)
- Capacitor runtime
- BLE Peripheral plugin (Java code)
- Web assets from `assets/public/` (peripheral demo)

**7e. Copy APK:**
```bash
mkdir -p ../build-output
cp android/app/build/outputs/apk/debug/app-debug.apk ../build-output/ble-peripheral-demo-debug.apk
```
- Copies APK to build output folder with descriptive name

**7f. Restore original dist (AFTER build):**
```bash
mv dist dist-peripheral
mv dist-backup dist
```
- Restores folder structure to original state
- `dist/` = main app again
- `dist-peripheral/` = peripheral demo again

**Final folder state:**
```
webclient/
  ├── dist/                 # ← Main app (restored)
  ├── dist-peripheral/      # ← Peripheral demo (restored)
  └── android/
      └── app/src/main/assets/public/  # ← Still has peripheral demo (from sync)

build-output/
  └── ble-peripheral-demo-debug.apk  # ← Contains peripheral demo!
```

---

### STEP 8: Rename APK (Redundant)

**This step has been removed** - it was redundant since the APK was already copied in Step 7.

---

## Issues Found and Fixed

### Issue 1: Restore Happening Before Build (FIXED)
The original codemagic.yaml had the dist folder restore happening BEFORE the Gradle build, causing the APK to contain the wrong web assets.

**Fix:** Moved the restore commands to AFTER `./gradlew assembleDebug`

### Issue 2: Redundant APK Copy Step (FIXED)
Step 8 was copying the same APK file again, which was unnecessary.

**Fix:** Removed the redundant "Rename APK" step entirely.

### Issue 3: Capacitor Config Not Used During Sync
The peripheral demo has its own capacitor config (`capacitor-peripheral.config.json`) with the correct app ID and name, but `npx cap sync android` uses the default `capacitor.config.json`.

**Current State:** This is actually OK because:
- The Android project is already created with the correct structure
- We only need to swap the web assets (which happens correctly)
- The app ID and name are set during initial `npx cap add android`

**No fix needed** - the current approach works.

---

## Summary

### What Gets Built:

1. **BLE Peripheral Plugin** (Step 2)
   - Location: `node_modules/@superfede/capacitor-ble-peripheral/`
   - Contains: TypeScript definitions + Android Java implementation
   - Purpose: Adds BLE server/peripheral functionality to Capacitor

2. **Main Webclient App** (Step 4)
   - Location: `webclient/dist/`
   - Entry: `index.html` → `main.jsx` → `App.jsx`
   - Purpose: Full relay timer controller UI (Dashboard, Schedule, etc.)
   - BLE Mode: CLIENT (connects to ESP32 devices)

3. **Peripheral Demo App** (Step 6)
   - Location: `webclient/dist-peripheral/`
   - Entry: `index-peripheral.html` → `main-peripheral.jsx` → `AppPeripheralDemo.jsx`
   - Purpose: Simple test UI for BLE peripheral functionality
   - BLE Mode: SERVER (Android acts as BLE device)

4. **Android Project** (Step 5)
   - Location: `webclient/android/`
   - Contains: Complete Android app structure with Capacitor runtime
   - Plugins: Includes both BLE client and BLE peripheral plugins

5. **Final APK** (Step 7)
   - Location: `build-output/ble-peripheral-demo-debug.apk`
   - Contains: Android app + Capacitor + BLE Peripheral plugin + Peripheral demo web assets
   - When launched: Shows BLEPeripheralDemo component

### Why Build Main App First?

The main app is built first (Step 4) because:
- Capacitor requires a valid `dist/` folder before running `npx cap add android`
- The Android platform initialization needs web assets to set up properly
- It doesn't matter WHAT is in `dist/` initially - it gets overwritten in Step 7

### The Key Fix (Step 7):

**OLD (BROKEN) ORDER:**
1. Swap dist folders
2. Sync Capacitor (copies peripheral demo)
3. **Restore dist folders** ← WRONG! Happens before build
4. Build APK ← Uses wrong assets

**NEW (FIXED) ORDER:**
1. Swap dist folders
2. Sync Capacitor (copies peripheral demo)
3. Build APK ← Uses peripheral demo assets
4. **Restore dist folders** ← Happens after build

The restore must happen AFTER the APK is built, not before!

---

## Verification Checklist

When the APK is installed on your Android device, you should see:

✅ **App Name:** "BLE Peripheral Demo" (not "BLE Timer")  
✅ **Package ID:** net.superfede.BlePeripheralDemo  
✅ **UI:** Simple test interface with BLEPeripheralDemo component  
✅ **Functionality:** Start/Stop Advertising buttons, characteristic value display  
✅ **No Dashboard:** Should NOT show the relay timer dashboard  
✅ **No Schedule Grid:** Should NOT show the weekly schedule editor  

If you see the relay timer UI instead, the wrong web assets were packaged.

---

## Next Steps After Build

1. Download `ble-peripheral-demo-debug.apk` from Codemagic artifacts
2. Install on Android device (API 24+)
3. Grant Bluetooth permissions when prompted
4. Test BLE peripheral functionality:
   - Tap "Start Advertising"
   - Device should appear as "ESP32 Timer" in BLE scanners
   - Connect from another device/laptop
   - Read/write characteristics
   - Verify notifications work

---

## Troubleshooting

**If APK still contains wrong app:**
1. Check `android/app/src/main/assets/public/index.html`
2. Look for `<title>BLE Peripheral Demo</title>` (correct) vs `<title>ESP32 Relay Controller</title>` (wrong)
3. Verify the JavaScript bundle references `AppPeripheralDemo` not `App`

**If build fails:**
1. Check that `dist-peripheral/` exists before Step 7
2. Verify `index-peripheral.html` exists in source
3. Ensure plugin is installed correctly in Step 2

