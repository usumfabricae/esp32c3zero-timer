# Codemagic Android Build Setup

This guide explains how to set up automated Android app builds using Codemagic CI/CD.

## Overview

The Codemagic workflow automatically:
1. Installs npm dependencies
2. Initializes Capacitor (if needed)
3. Adds Android platform (if needed)
4. Builds the React web app
5. Syncs Capacitor with Android
6. Builds the Android APK

## Files Created

- `codemagic.yaml` - Codemagic CI/CD configuration
- `webclient/capacitor.config.ts` - Capacitor configuration with your app ID

## App Configuration

- **Package Name:** `net.superfede.BleTimerWebView`
- **App Name:** BLE Timer
- **Web Directory:** `dist` (Vite build output)

## Setup Steps

### 1. Connect Repository to Codemagic

1. Go to [codemagic.io](https://codemagic.io)
2. Sign up or log in
3. Click "Add application"
4. Connect your Git repository (GitHub, GitLab, or Bitbucket)
5. Select your repository

### 2. Configure Workflow

Codemagic will automatically detect the `codemagic.yaml` file.

1. In Codemagic dashboard, select your app
2. Go to "Workflow settings"
3. Verify the workflow is detected
4. Update environment variables if needed:
   - `PACKAGE_NAME`: net.superfede.BleTimerWebView
   - `APP_NAME`: BLE Timer

### 3. Set Up Android Signing (Optional)

For release builds, you need to sign your APK:

#### Generate Keystore (if you don't have one)

```bash
keytool -genkey -v -keystore my-release-key.keystore \
  -alias my-key-alias -keyalg RSA -keysize 2048 -validity 10000
```

#### Upload to Codemagic

1. In Codemagic dashboard → "Code signing identities"
2. Click "Android" → "Upload keystore"
3. Upload your `.keystore` file
4. Enter keystore password and alias
5. Save as `keystore_reference`

### 4. Configure Email Notifications

Update `codemagic.yaml`:

```yaml
publishing:
  email:
    recipients:
      - your-email@example.com  # Change this
    notify:
      success: true
      failure: true
```

### 5. Trigger Build

Builds trigger automatically on:
- Push to `main` branch
- Pull requests
- Git tags

Or manually:
1. Go to Codemagic dashboard
2. Select your app
3. Click "Start new build"

## Build Output

After successful build, you'll find:
- **APK:** `webclient/android/app/build/outputs/apk/release/app-release.apk`
- **AAB:** `webclient/android/app/build/outputs/bundle/release/app-release.aab`

Download from Codemagic artifacts section.

## Workflow Details

### Build Steps

1. **Install npm dependencies**
   - Runs `npm ci` for clean install
   - Uses package-lock.json for reproducible builds

2. **Initialize Capacitor**
   - Only runs if `capacitor.config.ts` doesn't exist
   - Uses environment variables for app ID and name

3. **Add Android platform**
   - Only runs if `android/` folder doesn't exist
   - Downloads Android platform files

4. **Build web app**
   - Runs `npm run build`
   - Outputs to `dist/` folder

5. **Sync Capacitor**
   - Copies web assets to Android project
   - Updates native configuration

6. **Build Android APK**
   - Runs Gradle build
   - Produces release APK

### Environment

- **Node.js:** 18
- **Java:** 17
- **Instance:** mac_mini_m1 (M1 Mac for faster builds)
- **Max Duration:** 60 minutes

## Troubleshooting

### Build Fails at "Initialize Capacitor"

**Solution:** The `capacitor.config.ts` file should already exist in your repo. If not, create it:

```typescript
import { CapacitorConfig } from '@capacitor/cli';

const config: CapacitorConfig = {
  appId: 'net.superfede.BleTimerWebView',
  appName: 'BLE Timer',
  webDir: 'dist',
  server: {
    androidScheme: 'https'
  }
};

export default config;
```

### Build Fails at "Add Android platform"

**Error:** "Android platform already exists"

**Solution:** This is expected if `android/` folder is in your repo. The script checks for this and skips if exists.

### Build Fails at Gradle

**Error:** "SDK location not found"

**Solution:** The script creates `local.properties` automatically. If it still fails, check that `ANDROID_SDK_ROOT` is set in Codemagic environment.

### APK Not Signed

**Issue:** APK is debug-signed, not release-signed

**Solution:** Set up Android signing in Codemagic (see step 3 above).

### Web Bluetooth Not Working in APK

**Issue:** Web Bluetooth API doesn't work in the installed app

**Solution:** This is expected. Web Bluetooth only works in browsers, not in WebView. For native Bluetooth, you need to:

1. Install Bluetooth LE plugin:
   ```bash
   npm install @capacitor-community/bluetooth-le
   ```

2. Refactor `useBLE.js` to use the plugin instead of Web Bluetooth API

3. See Capacitor Bluetooth LE documentation for implementation

## Publishing to Google Play Store

### 1. Create App in Play Console

1. Go to [Google Play Console](https://play.google.com/console)
2. Create new app
3. Fill in app details

### 2. Configure Codemagic Publishing

Add to `codemagic.yaml`:

```yaml
publishing:
  google_play:
    credentials: $GCLOUD_SERVICE_ACCOUNT_CREDENTIALS
    track: internal  # or alpha, beta, production
    submit_as_draft: true
```

### 3. Set Up Service Account

1. In Google Cloud Console, create service account
2. Download JSON key
3. In Codemagic, add as environment variable: `GCLOUD_SERVICE_ACCOUNT_CREDENTIALS`

### 4. Build AAB Instead of APK

Update Gradle command in `codemagic.yaml`:

```yaml
- name: Build Android AAB
  script: |
    cd webclient/android
    ./gradlew bundleRelease
```

## Advanced Configuration

### Custom Build Variants

To build different variants (debug, staging, production):

```yaml
- name: Build Android (staging)
  script: |
    cd webclient/android
    ./gradlew assembleStaging
```

### Version Management

Update version in `webclient/android/app/build.gradle`:

```gradle
android {
    defaultConfig {
        versionCode 1
        versionName "1.0.0"
    }
}
```

Or automate with environment variables in Codemagic.

### Cache Dependencies

Speed up builds by caching:

```yaml
cache:
  cache_paths:
    - $HOME/.gradle/caches
    - $HOME/.npm
    - webclient/node_modules
```

## Next Steps

1. Commit `codemagic.yaml` and `capacitor.config.ts`
2. Push to GitHub
3. Connect repository to Codemagic
4. Trigger first build
5. Download and test APK

## Resources

- [Codemagic Documentation](https://docs.codemagic.io/)
- [Capacitor Documentation](https://capacitorjs.com/docs)
- [Android Signing Guide](https://docs.codemagic.io/yaml-code-signing/signing-android/)
