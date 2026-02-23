# PWA Installation Troubleshooting

## Issue: No "Install" Button Appears

Your PWA won't show an install prompt if any of these conditions aren't met:

### 1. Check Icon Files Exist

Both icons MUST exist in `public/` folder:
```bash
cd webclient/public
ls icon-*.png
# Should show: icon-192.png and icon-512.png
```

**Fix if missing:**
```bash
cd webclient
node create-pwa-icons.js
# Then convert the SVG files to PNG (see below)
```

### 2. Verify Icons Are Accessible

Open your browser and check these URLs directly:
- `https://YOUR_GITHUB_USERNAME.github.io/esp32c3zero-timer/icon-192.png`
- `https://YOUR_GITHUB_USERNAME.github.io/esp32c3zero-timer/icon-512.png`

Both should display the icon image. If you get 404 errors, the icons weren't included in the build.

### 3. Check Manifest Is Valid

Open Chrome DevTools (F12) → Application tab → Manifest

Look for errors like:
- ❌ "No matching service worker detected"
- ❌ "Manifest does not have a valid icon"
- ❌ "start_url does not respond with 200 when offline"

### 4. Verify Service Worker Is Running

Chrome DevTools → Application tab → Service Workers

Should show:
- ✅ Status: "activated and running"
- ✅ Source: `/esp32c3zero-timer/sw.js`

If not running, check browser console for errors.

### 5. Check HTTPS

PWA requires HTTPS. GitHub Pages provides this automatically, but verify:
- URL starts with `https://`
- No certificate warnings in browser

### 6. Test PWA Installability

Chrome DevTools → Application tab → Manifest → "Add to home screen" link

Click it to manually trigger the install prompt. If it doesn't work, check the errors shown.

## Common Issues & Solutions

### Issue: Icons Show as Broken Images

**Cause:** Icon files are missing or wrong format

**Solution:**
1. Generate proper PNG icons:
   ```bash
   cd webclient
   # Option 1: Use the icon generator HTML
   start generate-icons.html
   # Download both icons and save to public/
   
   # Option 2: Create SVG icons
   node create-pwa-icons.js
   # Then convert SVG to PNG using online tool or image editor
   ```

2. Verify icon dimensions:
   - icon-192.png must be exactly 192x192 pixels
   - icon-512.png must be exactly 512x512 pixels

3. Commit and push:
   ```bash
   git add webclient/public/icon-*.png
   git commit -m "Add PWA icons"
   git push
   ```

### Issue: "Add to Home Screen" Not in Browser Menu

**Cause:** Browser doesn't think app is installable

**Check:**
1. All PWA criteria met (see checklist above)
2. Not already installed (uninstall first if testing)
3. Using supported browser (Chrome/Edge on Android)
4. Not in incognito/private mode

**Android Chrome Specific:**
- Menu → "Install app" or "Add to Home Screen"
- If not visible, app doesn't meet PWA criteria

### Issue: Service Worker Not Registering

**Symptoms:** Console shows "SW registration failed"

**Solutions:**
1. Check `sw.js` is in `public/` folder
2. Verify `sw.js` has no syntax errors
3. Clear browser cache and reload
4. Check browser console for specific error

### Issue: Icons Wrong After Installation

**Cause:** Browser cached old icons

**Solution:**
1. Uninstall the app
2. Clear browser cache
3. Update cache version in `sw.js`:
   ```javascript
   const CACHE_NAME = 'esp32-relay-v2'; // Increment version
   ```
4. Rebuild and redeploy
5. Reinstall app

### Issue: App Installs But Won't Open

**Cause:** start_url in manifest is incorrect

**Solution:**
1. Check `manifest.json`:
   ```json
   {
     "start_url": "/esp32c3zero-timer/",
     "scope": "/esp32c3zero-timer/"
   }
   ```

2. Verify these match your GitHub Pages URL structure

3. Test by visiting the start_url directly in browser

## Creating Proper PNG Icons

### Method 1: Online Converter (Easiest)

1. Generate SVG icons:
   ```bash
   cd webclient
   node create-pwa-icons.js
   ```

2. Visit https://cloudconvert.com/svg-to-png

3. Upload `public/icon-192.svg` and `public/icon-512.svg`

4. Convert and download as PNG

5. Save to `webclient/public/` folder

### Method 2: Using Browser (Quick)

1. Open `generate-icons.html` in Chrome

2. Right-click each canvas → "Save image as..."

3. Save as `icon-192.png` and `icon-512.png`

4. Move to `webclient/public/` folder

### Method 3: Image Editor (Best Quality)

1. Open Photoshop, GIMP, or similar

2. Create new image:
   - 192x192 pixels for first icon
   - 512x512 pixels for second icon

3. Design your icon (or paste from SVG)

4. Export as PNG

5. Save to `webclient/public/` folder

## Verification Checklist

Before pushing to GitHub, verify locally:

```bash
cd webclient
npm run build
npm run preview
```

Then check:
- [ ] Visit `https://localhost:4173/esp32c3zero-timer/`
- [ ] Open DevTools → Application → Manifest (no errors)
- [ ] Both icons visible in manifest
- [ ] Service worker shows "activated and running"
- [ ] Click "Add to home screen" link in DevTools
- [ ] Install prompt appears
- [ ] App installs successfully
- [ ] App opens in standalone mode

## Testing on Android

1. Push changes to GitHub

2. Wait for GitHub Actions to complete

3. On Android device:
   - Open Chrome
   - Visit your GitHub Pages URL
   - Wait 5-10 seconds
   - Look for install banner at bottom
   - OR tap menu (⋮) → "Install app"

4. If no install option:
   - Open DevTools on desktop Chrome
   - Use Remote Debugging to inspect mobile browser
   - Check Application tab for errors

## Still Not Working?

### Debug Steps:

1. **Check GitHub Actions build log:**
   - Go to your repo → Actions tab
   - Check if build succeeded
   - Verify `public/` folder contents were copied to `dist/`

2. **Inspect deployed files:**
   - Visit `https://YOUR_USERNAME.github.io/esp32c3zero-timer/manifest.json`
   - Should show your manifest content
   - Visit icon URLs (see step 2 above)

3. **Test with Lighthouse:**
   - Chrome DevTools → Lighthouse tab
   - Run "Progressive Web App" audit
   - Fix any issues reported

4. **Check browser compatibility:**
   - PWA install only works on:
     - Chrome 56+ (Android/Desktop)
     - Edge 79+ (Android/Desktop)
     - Opera 43+ (Android/Desktop)
   - Does NOT work on:
     - Firefox (no Web Bluetooth)
     - Safari/iOS (no Web Bluetooth)

## Alternative: Manual "Add to Home Screen"

If automatic install prompt doesn't work, users can manually add:

**Android Chrome:**
1. Tap menu (⋮)
2. Select "Add to Home Screen"
3. Confirm

This works even without full PWA criteria, but won't have all PWA features.

## Need More Help?

Check these resources:
- [PWA Checklist](https://web.dev/pwa-checklist/)
- [Web App Manifest](https://web.dev/add-manifest/)
- [Service Workers](https://web.dev/service-workers-cache-storage/)
- [Debugging PWAs](https://web.dev/debug-pwas/)
