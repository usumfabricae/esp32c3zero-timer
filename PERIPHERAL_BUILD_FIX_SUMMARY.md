# Peripheral Demo Build Fix Summary

## Problem
The Codemagic build was producing an APK that contained the main webclient app (relay timer) instead of the peripheral demo app.

## Root Cause
In the "Build Peripheral Demo APK" step, the dist folder restore was happening BEFORE the Gradle build:

```bash
# WRONG ORDER:
1. Swap dist folders (peripheral demo → dist)
2. npx cap sync android (copies peripheral demo)
3. Restore dist folders (main app → dist)  ← WRONG!
4. ./gradlew assembleDebug (builds APK)     ← Uses wrong assets
```

Even though `npx cap sync` copied the peripheral demo to `android/app/src/main/assets/public/`, the restore step was confusing and the build was using cached or wrong assets.

## Solution
Move the restore to AFTER the Gradle build:

```bash
# CORRECT ORDER:
1. Swap dist folders (peripheral demo → dist)
2. npx cap sync android (copies peripheral demo)
3. ./gradlew assembleDebug (builds APK)     ← Uses peripheral demo assets
4. Restore dist folders (main app → dist)   ← Happens after build
```

## Changes Made

### 1. Fixed codemagic.yaml
**File:** `codemagic.yaml`  
**Section:** `android-peripheral-demo` workflow, "Build Peripheral Demo APK" step

**Before:**
```yaml
- name: Build Peripheral Demo APK
  script: |
    cd webclient
    mv dist dist-backup
    mv dist-peripheral dist
    npx cap sync android
    
    # Restore BEFORE build (WRONG!)
    mv dist dist-peripheral
    mv dist-backup dist
    
    # Build APK
    cd android
    ./gradlew assembleDebug
```

**After:**
```yaml
- name: Build Peripheral Demo APK
  script: |
    cd webclient
    mv dist dist-backup
    mv dist-peripheral dist
    npx cap sync android
    
    # Build APK FIRST
    cd android
    ./gradlew assembleDebug
    cd ..
    
    # Copy APK
    mkdir -p ../build-output
    cp android/app/build/outputs/apk/debug/app-debug.apk ../build-output/ble-peripheral-demo-debug.apk
    
    # Restore AFTER build (CORRECT!)
    mv dist dist-peripheral
    mv dist-backup dist
```

### 2. Removed Redundant Step
**Removed:** "Rename APK" step (Step 8)  
**Reason:** The APK was already copied in Step 7, making this step redundant.

### 3. Updated Documentation
**File:** `CODEMAGIC_BUILD_FLOW.md`  
**Added:**
- Detailed explanation of the fix
- Issues found and fixed section
- Verification checklist
- Troubleshooting guide

## Expected Result

When you push to the `reverse` branch, Codemagic will:
1. Build both web apps (main + peripheral demo)
2. Create Android project with main app initially
3. Swap in peripheral demo assets
4. Build APK with peripheral demo
5. Produce: `ble-peripheral-demo-debug.apk`

The APK should contain:
- App name: "BLE Peripheral Demo"
- Package: net.superfede.BlePeripheralDemo
- UI: BLEPeripheralDemo component (simple test interface)
- Functionality: BLE peripheral/server mode

## Verification

After installing the APK on your Android device:

✅ **Correct:** Shows "BLE Peripheral Demo" title and simple test UI  
❌ **Wrong:** Shows "ESP32 Relay Controller" with dashboard and schedule grid

## Files Modified
1. `codemagic.yaml` - Fixed build order, removed redundant step
2. `CODEMAGIC_BUILD_FLOW.md` - Added detailed documentation
3. `PERIPHERAL_BUILD_FIX_SUMMARY.md` - This file

## Next Steps
1. Commit and push changes to `reverse` branch
2. Wait for Codemagic build to complete
3. Download `ble-peripheral-demo-debug.apk` from artifacts
4. Install on Android device
5. Test BLE peripheral functionality
