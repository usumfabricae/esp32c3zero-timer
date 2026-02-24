#!/usr/bin/env pwsh
# Build script for BLE Peripheral Demo APK

Write-Host "Building BLE Peripheral Demo APK..." -ForegroundColor Cyan

# Navigate to webclient directory
Set-Location $PSScriptRoot

# Install dependencies if needed
if (-not (Test-Path "node_modules")) {
    Write-Host "Installing npm dependencies..." -ForegroundColor Yellow
    npm install
}

# Install BLE peripheral plugin
Write-Host "Installing BLE Peripheral plugin..." -ForegroundColor Yellow
npm install ./ble-peripheral-plugin

# Build the peripheral demo web app
Write-Host "Building peripheral demo web app..." -ForegroundColor Yellow
$env:CAPACITOR = "true"
npm run build:peripheral

# Check if build succeeded
if (-not (Test-Path "dist-peripheral")) {
    Write-Host "Build failed - dist-peripheral directory not found" -ForegroundColor Red
    exit 1
}

# Initialize Capacitor with peripheral config if needed
if (-not (Test-Path "android-peripheral")) {
    Write-Host "Initializing Capacitor for peripheral demo..." -ForegroundColor Yellow
    
    # Add Android platform with custom config
    npx cap add android --capacitor-config capacitor-peripheral.config.json
    
    # Rename android folder
    if (Test-Path "android") {
        Move-Item "android" "android-peripheral" -Force
    }
}

# Sync Capacitor
Write-Host "Syncing Capacitor..." -ForegroundColor Yellow
npx cap sync android --capacitor-config capacitor-peripheral.config.json

# Ensure android-peripheral folder exists
if (-not (Test-Path "android-peripheral")) {
    Write-Host "Error: android-peripheral folder not found" -ForegroundColor Red
    exit 1
}

# Build the APK
Write-Host "Building Android APK..." -ForegroundColor Yellow
Set-Location android-peripheral

if ($IsWindows) {
    .\gradlew.bat assembleDebug
} else {
    chmod +x gradlew
    ./gradlew assembleDebug
}

# Check if APK was created
$apkPath = "app/build/outputs/apk/debug/app-debug.apk"
if (Test-Path $apkPath) {
    Write-Host "`nBuild successful!" -ForegroundColor Green
    Write-Host "APK location: android-peripheral/$apkPath" -ForegroundColor Cyan
    
    # Copy to root for easy access
    $destPath = "../ble-peripheral-demo.apk"
    Copy-Item $apkPath $destPath -Force
    Write-Host "APK copied to: webclient/ble-peripheral-demo.apk" -ForegroundColor Cyan
} else {
    Write-Host "`nBuild failed - APK not found" -ForegroundColor Red
    exit 1
}

Set-Location ..
Write-Host "`nDone! Install the APK on your Android device to test." -ForegroundColor Green
