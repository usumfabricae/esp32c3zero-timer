#!/bin/bash
# Verification script to check peripheral demo build

echo "=== Peripheral Demo Build Verification ==="
echo ""

# Check if dist-peripheral exists
if [ -d "dist-peripheral" ]; then
    echo "✓ dist-peripheral directory exists"
    
    # Check index.html
    if [ -f "dist-peripheral/index.html" ]; then
        echo "✓ dist-peripheral/index.html exists"
        
        # Check if it references the peripheral entry point
        if grep -q "main-peripheral" dist-peripheral/index.html; then
            echo "✓ index.html references main-peripheral.jsx"
        else
            echo "✗ index.html does NOT reference main-peripheral.jsx"
            echo "  Content:"
            cat dist-peripheral/index.html
        fi
    else
        echo "✗ dist-peripheral/index.html NOT found"
    fi
    
    # List built assets
    echo ""
    echo "Built assets:"
    ls -lh dist-peripheral/assets/ 2>/dev/null || echo "  No assets found"
else
    echo "✗ dist-peripheral directory NOT found"
fi

echo ""

# Check android-peripheral
if [ -d "android-peripheral" ]; then
    echo "✓ android-peripheral directory exists"
    
    # Check package name in build.gradle
    if [ -f "android-peripheral/app/build.gradle" ]; then
        echo "✓ android-peripheral/app/build.gradle exists"
        
        # Check applicationId
        APP_ID=$(grep "applicationId" android-peripheral/app/build.gradle | head -1)
        echo "  $APP_ID"
        
        if echo "$APP_ID" | grep -q "BlePeripheralDemo"; then
            echo "✓ Application ID is correct (BlePeripheralDemo)"
        else
            echo "✗ Application ID is NOT BlePeripheralDemo"
        fi
    fi
    
    # Check AndroidManifest.xml
    if [ -f "android-peripheral/app/src/main/AndroidManifest.xml" ]; then
        echo "✓ AndroidManifest.xml exists"
        
        if grep -q "BLUETOOTH_ADVERTISE" android-peripheral/app/src/main/AndroidManifest.xml; then
            echo "✓ BLUETOOTH_ADVERTISE permission found"
        else
            echo "✗ BLUETOOTH_ADVERTISE permission NOT found"
        fi
    fi
    
    # Check if APK was built
    if [ -f "android-peripheral/app/build/outputs/apk/debug/app-debug.apk" ]; then
        echo "✓ APK exists"
        APK_SIZE=$(ls -lh android-peripheral/app/build/outputs/apk/debug/app-debug.apk | awk '{print $5}')
        echo "  Size: $APK_SIZE"
    else
        echo "✗ APK NOT found"
    fi
else
    echo "✗ android-peripheral directory NOT found"
fi

echo ""
echo "=== End Verification ==="
