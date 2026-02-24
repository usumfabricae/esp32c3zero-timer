# AWS CodeBuild Setup for Android Builds

This guide explains how to set up AWS CodeBuild to build your Android APKs as an alternative to Codemagic.

## Recommended Docker Images for Android

AWS CodeBuild doesn't provide official Android images, but you can use these community-maintained options:

### Option 1: ReactNativeCI Android Image (Recommended)
```
Image: reactnativecommunity/react-native-android:latest
```
- Pre-installed: Android SDK 34, Node.js, Java 17
- Optimized for React Native/Capacitor builds
- Well-maintained by React Native community
- [Docker Hub](https://hub.docker.com/r/reactnativecommunity/react-native-android)

### Option 2: Thyrlian Android SDK Image
```
Image: thyrlian/android-sdk:latest
```
- Pre-installed: Android SDK, Java
- Lightweight and focused
- [Docker Hub](https://hub.docker.com/r/thyrlian/android-sdk)

### Option 3: Mingc Android Build Image
```
Image: mingc/android-build-box:latest
```
- Pre-installed: Android SDK 34, Node.js, Java 21
- Includes multiple Android SDK versions
- [Docker Hub](https://hub.docker.com/r/mingc/android-build-box)

## Setup Steps

### 1. Create CodeBuild Project

#### Via AWS Console

1. Go to [AWS CodeBuild Console](https://console.aws.amazon.com/codebuild/)
2. Click "Create build project"
3. Configure:

**Project configuration:**
- Project name: `android-ble-timer-build`
- Description: `Build Android APKs for BLE Timer app`

**Source:**
- Source provider: GitHub
- Repository: Your repository URL
- Branch: `main`
- Enable webhook for automatic builds

**Environment:**
- Environment image: `Custom image`
- Image type: `Linux`
- Custom image type: `Other registry`
- External registry URL: `reactnativecommunity/react-native-android:latest`
- Compute: `3 GB memory, 2 vCPUs` (or higher for faster builds)
- Service role: Create new or use existing

**Buildspec:**
- Build specifications: `Use a buildspec file`
- Buildspec name: `buildspec.yml`

**Artifacts:**
- Type: `Amazon S3`
- Bucket name: Create or select S3 bucket
- Name: `android-apks`
- Artifacts packaging: `Zip`

**Logs:**
- CloudWatch logs: Enabled
- S3 logs: Optional

4. Click "Create build project"

#### Via AWS CLI

```bash
aws codebuild create-project \
  --name android-ble-timer-build \
  --source type=GITHUB,location=https://github.com/YOUR_USERNAME/esp32c3zero-timer.git \
  --artifacts type=S3,location=YOUR_BUCKET_NAME,packaging=ZIP \
  --environment type=LINUX_CONTAINER,image=reactnativecommunity/react-native-android:latest,computeType=BUILD_GENERAL1_MEDIUM \
  --service-role arn:aws:iam::YOUR_ACCOUNT_ID:role/CodeBuildServiceRole
```

### 2. Configure IAM Permissions

Your CodeBuild service role needs these permissions:

```json
{
  "Version": "2012-10-17",
  "Statement": [
    {
      "Effect": "Allow",
      "Action": [
        "logs:CreateLogGroup",
        "logs:CreateLogStream",
        "logs:PutLogEvents"
      ],
      "Resource": "arn:aws:logs:*:*:*"
    },
    {
      "Effect": "Allow",
      "Action": [
        "s3:PutObject",
        "s3:GetObject",
        "s3:GetObjectVersion"
      ],
      "Resource": "arn:aws:s3:::YOUR_BUCKET_NAME/*"
    },
    {
      "Effect": "Allow",
      "Action": [
        "s3:ListBucket"
      ],
      "Resource": "arn:aws:s3:::YOUR_BUCKET_NAME"
    }
  ]
}
```

### 3. Set Up GitHub Webhook (Optional)

For automatic builds on push:

1. In CodeBuild project settings
2. Go to "Source" section
3. Enable "Webhook"
4. Select events: `PUSH`, `PULL_REQUEST_CREATED`, `PULL_REQUEST_UPDATED`
5. Save

Or via CLI:
```bash
aws codebuild create-webhook \
  --project-name android-ble-timer-build \
  --filter-groups '[[{"type":"EVENT","pattern":"PUSH"}]]'
```

### 4. Configure Build Triggers

Create separate projects for different workflows:

**Main App Build:**
- Project: `android-ble-timer-main`
- Branch filter: `main`
- Buildspec: `buildspec.yml`

**Peripheral Demo Build:**
- Project: `android-ble-peripheral-demo`
- Branch filter: `main,peripheral-demo`
- Buildspec: `buildspec-peripheral.yml`

## Build Configuration

### buildspec.yml

The `buildspec.yml` file in the root directory defines the build process:

```yaml
version: 0.2

phases:
  install:
    commands:
      - echo "Using pre-built Android Docker image"
      - node --version
      - npm --version
      
  build:
    commands:
      - cd webclient
      - npm ci
      - CAPACITOR=true npm run build
      - npx cap sync android
      - cd android && ./gradlew assembleDebug
      
artifacts:
  files:
    - 'build-output/*.apk'
```

### Environment Variables

Set in CodeBuild project:

```bash
NODE_VERSION=22
ANDROID_HOME=/opt/android-sdk
```

### Secrets (Optional)

For signing release builds, store in AWS Systems Manager Parameter Store:

```bash
aws ssm put-parameter \
  --name /codebuild/android/keystore-password \
  --value "YOUR_PASSWORD" \
  --type SecureString

aws ssm put-parameter \
  --name /codebuild/android/key-password \
  --value "YOUR_PASSWORD" \
  --type SecureString
```

Reference in buildspec.yml:
```yaml
env:
  parameter-store:
    KEYSTORE_PASSWORD: /codebuild/android/keystore-password
    KEY_PASSWORD: /codebuild/android/key-password
```

## Running Builds

### Manual Build

Via Console:
1. Go to CodeBuild project
2. Click "Start build"
3. Select branch
4. Click "Start build"

Via CLI:
```bash
aws codebuild start-build \
  --project-name android-ble-timer-build
```

### Automatic Builds

Builds trigger automatically on:
- Push to `main` branch
- Pull request created/updated
- Tag created

## Monitoring Builds

### CloudWatch Logs

View build logs in CloudWatch:
```bash
aws logs tail /aws/codebuild/android-ble-timer-build --follow
```

### Build Status

Check build status:
```bash
aws codebuild batch-get-builds \
  --ids android-ble-timer-build:BUILD_ID
```

### Notifications

Set up SNS notifications for build status:

1. Create SNS topic
2. Subscribe email to topic
3. Add notification rule in CodeBuild project

## Downloading APKs

### From S3

```bash
aws s3 cp s3://YOUR_BUCKET_NAME/android-apks/ble-timer-debug.apk ./
```

### From Console

1. Go to CodeBuild project
2. Click on completed build
3. Go to "Artifacts" tab
4. Download APK

## Cost Estimation

### CodeBuild Pricing (US East)

- **Build time**: $0.005 per minute (general1.medium)
- **Storage**: S3 standard pricing
- **Data transfer**: Standard AWS rates

### Example Costs

**Scenario**: 100 builds/month, 10 minutes each
- Build time: 100 × 10 × $0.005 = $5.00
- S3 storage (10 GB): ~$0.23
- **Total**: ~$5.23/month

**vs Codemagic Free Tier**: 500 build minutes/month free

### Cost Optimization

1. **Use caching**: Cache node_modules and Gradle dependencies
2. **Faster compute**: Use larger instance for shorter builds
3. **Lifecycle policies**: Delete old artifacts from S3
4. **Build only on tags**: Reduce unnecessary builds

## Comparison: CodeBuild vs Codemagic

| Feature | AWS CodeBuild | Codemagic |
|---------|---------------|-----------|
| **Setup** | More complex | Simple |
| **Android SDK** | Custom image needed | Pre-installed |
| **Cost (low volume)** | ~$5/month | Free tier |
| **Cost (high volume)** | Lower | Higher |
| **Integration** | AWS ecosystem | Mobile-focused |
| **Flexibility** | High | Medium |
| **macOS/iOS** | Not supported | Supported |

## Troubleshooting

### Build Fails: Android SDK Not Found

**Solution**: Verify Docker image has Android SDK
```bash
# In buildspec.yml
- echo $ANDROID_HOME
- ls -la $ANDROID_HOME
```

### Build Fails: Out of Memory

**Solution**: Increase compute size
- Change to `BUILD_GENERAL1_LARGE` (7 GB memory)

### Build Fails: Gradle Daemon

**Solution**: Disable Gradle daemon
```bash
# In buildspec.yml
- cd android
- ./gradlew assembleDebug --no-daemon
```

### Node.js Version Wrong

**Solution**: Install specific Node.js version
```bash
# In buildspec.yml install phase
- curl -fsSL https://deb.nodesource.com/setup_22.x | bash -
- apt-get install -y nodejs
```

## Migration from Codemagic

1. **Keep Codemagic** for iOS builds (if needed)
2. **Use CodeBuild** for Android builds
3. **Test thoroughly** before switching completely
4. **Compare costs** after 1 month

## Next Steps

1. Create CodeBuild project
2. Test with manual build
3. Enable webhook for automatic builds
4. Set up S3 lifecycle policies
5. Configure notifications
6. Monitor costs

## Resources

- [AWS CodeBuild Documentation](https://docs.aws.amazon.com/codebuild/)
- [ReactNativeCI Docker Images](https://hub.docker.com/r/reactnativecommunity/react-native-android)
- [Android Build Box](https://hub.docker.com/r/mingc/android-build-box)
- [CodeBuild Pricing](https://aws.amazon.com/codebuild/pricing/)
