# PWA Setup Helper Script
# Helps set up PWA by opening icon generator and checking requirements

Write-Host "=== ESP32 Relay Controller - PWA Setup ===" -ForegroundColor Cyan
Write-Host ""

# Check if public directory exists
if (-not (Test-Path "public")) {
    Write-Host "Creating public directory..." -ForegroundColor Yellow
    New-Item -ItemType Directory -Path "public" | Out-Null
}

# Check if icons exist
$icon192 = Test-Path "public/icon-192.png"
$icon512 = Test-Path "public/icon-512.png"

if ($icon192 -and $icon512) {
    Write-Host "✓ App icons found" -ForegroundColor Green
} else {
    Write-Host "✗ App icons missing" -ForegroundColor Red
    Write-Host ""
    Write-Host "Opening icon generator in your browser..." -ForegroundColor Yellow
    Write-Host "1. Download both icons from the browser"
    Write-Host "2. Save them to the webclient/public/ folder"
    Write-Host ""
    Start-Process "generate-icons.html"
    Write-Host "Press any key after saving the icons..." -ForegroundColor Yellow
    $null = $Host.UI.RawUI.ReadKey("NoEcho,IncludeKeyDown")
}

# Check if manifest exists
if (Test-Path "public/manifest.json") {
    Write-Host "✓ Manifest file found" -ForegroundColor Green
} else {
    Write-Host "✗ Manifest file missing" -ForegroundColor Red
}

# Check if service worker exists
if (Test-Path "public/sw.js") {
    Write-Host "✓ Service worker found" -ForegroundColor Green
} else {
    Write-Host "✗ Service worker missing" -ForegroundColor Red
}

Write-Host ""
Write-Host "=== Next Steps ===" -ForegroundColor Cyan
Write-Host ""
Write-Host "1. Build the app:" -ForegroundColor White
Write-Host "   npm run build" -ForegroundColor Gray
Write-Host ""
Write-Host "2. Preview the build:" -ForegroundColor White
Write-Host "   npm run preview" -ForegroundColor Gray
Write-Host ""
Write-Host "3. On your Android device:" -ForegroundColor White
Write-Host "   - Visit https://YOUR_IP:4173/esp32c3zero-timer/" -ForegroundColor Gray
Write-Host "   - Tap browser menu (⋮)" -ForegroundColor Gray
Write-Host "   - Select 'Install app' or 'Add to Home Screen'" -ForegroundColor Gray
Write-Host ""
Write-Host "For detailed instructions, see PWA_SETUP.md" -ForegroundColor Yellow
Write-Host ""
