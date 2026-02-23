# PWA Setup Guide

Your web app now has Progressive Web App (PWA) capabilities!

## What Was Added

1. **manifest.json** - App metadata for installation
2. **sw.js** - Service worker for offline functionality
3. **Updated index.html** - Added PWA meta tags and service worker registration
4. **Icon generator** - Tool to create app icons

## Generate App Icons

You need to create two icon files: `icon-192.png` and `icon-512.png`

### Option 1: Use the Generator (Quick)
1. Open `webclient/generate-icons.html` in your browser
2. Click the download links to save both icons
3. Move the downloaded files to `webclient/public/`

### Option 2: Create Custom Icons
Create your own PNG images:
- `icon-192.png` - 192x192 pixels
- `icon-512.png` - 512x512 pixels

Place them in `webclient/public/`

## How to Install the PWA

### On Android (Chrome/Edge)

1. Build and serve your app:
   ```bash
   cd webclient
   npm run build
   npm run preview
   ```

2. Open the app on your Android device:
   - Visit `https://YOUR_IP:4173/esp32c3zero-timer/`
   - Accept the certificate warning (self-signed cert)

3. Install the app:
   - Tap the browser menu (⋮)
   - Select "Install app" or "Add to Home Screen"
   - Confirm installation

4. The app icon will appear on your home screen!

### On Desktop (Chrome/Edge)

1. Visit `https://localhost:4173/esp32c3zero-timer/`
2. Look for the install icon in the address bar (⊕)
3. Click "Install"

## Testing PWA Features

### Check Installation Eligibility
Open Chrome DevTools → Application → Manifest
- Should show no errors
- Icons should be visible

### Test Service Worker
Open Chrome DevTools → Application → Service Workers
- Should show "activated and running"
- Try going offline and reloading (basic caching works)

### Test Installation
1. Install the app
2. Open from home screen
3. Should open in standalone mode (no browser UI)
4. Web Bluetooth should work normally

## Important Notes

- **HTTPS Required**: PWA only works over HTTPS (you already have this)
- **Icons Required**: Generate the icons before testing installation
- **Android Only**: Web Bluetooth doesn't work on iOS PWAs
- **Network Access**: Device must be on same network as ESP32 for BLE
- **Certificate**: Users need to accept self-signed certificate once

## Updating the PWA

When you make changes:

1. Update version in `sw.js` (change `CACHE_NAME`)
2. Rebuild: `npm run build`
3. Users will get updates automatically on next visit

## Customization

### Change App Name
Edit `webclient/public/manifest.json`:
```json
{
  "name": "Your Custom Name",
  "short_name": "Short Name"
}
```

### Change Theme Colors
Edit `webclient/public/manifest.json`:
```json
{
  "theme_color": "#your-color",
  "background_color": "#your-color"
}
```

### Adjust Caching Strategy
Edit `webclient/public/sw.js` to change what gets cached offline

## Troubleshooting

**"Install" button doesn't appear**
- Check DevTools → Application → Manifest for errors
- Ensure icons exist in `public/` folder
- Must be served over HTTPS

**Service worker not registering**
- Check browser console for errors
- Ensure `sw.js` is in the `public/` folder
- Clear browser cache and reload

**App doesn't work offline**
- Service worker caches minimal files by default
- Full offline support requires caching all assets
- BLE requires device connection (can't work fully offline)

## Next Steps

1. Generate the icons using `generate-icons.html`
2. Build the app: `npm run build`
3. Test installation on Android device
4. Customize colors and names as needed

Your users can now install your ESP32 controller as a native-feeling app!
