# Quick IoT Core Testing Guide

Your device is currently running but **IoT Core integration is not active** because:
1. No certificates are provisioned
2. IoT Core initialization code is not integrated into main.c (Task 8)

## Option 1: Quick Test Without AWS (Recommended First)

To verify the IoT Core code compiles and initializes correctly without AWS:

### 1. Add IoT Core includes to main.c

Add these includes after the existing ones:
```c
#include "cert_store.h"
#include "iot_manager.h"
```

### 2. Add initialization in app_main()

After the NVS initialization, add:
```c
// Initialize IoT Core (will fail gracefully if no credentials)
ESP_LOGI(TAG, "Initializing IoT Core manager...");
esp_err_t iot_ret = iot_manager_init();
if (iot_ret == ESP_OK) {
    ESP_LOGI(TAG, "IoT Core manager initialized successfully");
} else {
    ESP_LOGI(TAG, "IoT Core not available (no credentials or disabled)");
}
```

### 3. Rebuild and flash

```powershell
.\build.ps1
.\deploy.ps1
```

### 4. Expected output

You should see:
```
I (xxxx) cert_store: Certificate store initialized
I (xxxx) cert_store: Credentials check: cert=0, key=0, ca=0
I (xxxx) iot_manager: IoT Core credentials not found, operating in BLE-only mode
I (xxxx) main: IoT Core not available (no credentials or disabled)
```

This confirms the code is working but needs credentials.

## Option 2: Full AWS IoT Core Setup

Follow the complete guide in **README-IOTCORE.md**. Here's the condensed version:

### Step 1: Get AWS IoT Endpoint

```bash
aws iot describe-endpoint --endpoint-type iot:Data-ATS
```

Update `main/config.h`:
```c
#define IOT_ENDPOINT "a1b2c3d4e5f6g7-ats.iot.us-east-1.amazonaws.com"
```

### Step 2: Create Thing and Certificates

```bash
# Create thing (use your device's actual ID from serial logs)
aws iot create-thing --thing-name esp32timer-f76c5c

# Generate certificates
aws iot create-keys-and-certificate \
    --set-as-active \
    --certificate-pem-outfile device.cert.pem \
    --public-key-outfile device.public.key \
    --private-key-outfile device.private.key

# Download AWS Root CA
curl -o AmazonRootCA1.pem https://www.amazontrust.com/repository/AmazonRootCA1.pem
```

**Save the certificateArn from the output!**

### Step 3: Create and Attach Policy

Create `policy.json`:
```json
{
  "Version": "2012-10-17",
  "Statement": [
    {
      "Effect": "Allow",
      "Action": ["iot:Connect"],
      "Resource": ["arn:aws:iot:REGION:ACCOUNT_ID:client/esp32timer-*"]
    },
    {
      "Effect": "Allow",
      "Action": ["iot:Publish"],
      "Resource": [
        "arn:aws:iot:REGION:ACCOUNT_ID:topic/esp32timer/*/status/*",
        "arn:aws:iot:REGION:ACCOUNT_ID:topic/$aws/things/esp32timer-*/shadow/update"
      ]
    },
    {
      "Effect": "Allow",
      "Action": ["iot:Subscribe"],
      "Resource": [
        "arn:aws:iot:REGION:ACCOUNT_ID:topicfilter/esp32timer/*/commands/#",
        "arn:aws:iot:REGION:ACCOUNT_ID:topicfilter/$aws/things/esp32timer-*/shadow/update/delta"
      ]
    },
    {
      "Effect": "Allow",
      "Action": ["iot:Receive"],
      "Resource": [
        "arn:aws:iot:REGION:ACCOUNT_ID:topic/esp32timer/*/commands/*",
        "arn:aws:iot:REGION:ACCOUNT_ID:topic/$aws/things/esp32timer-*/shadow/update/delta"
      ]
    },
    {
      "Effect": "Allow",
      "Action": ["iot:GetThingShadow", "iot:UpdateThingShadow"],
      "Resource": ["arn:aws:iot:REGION:ACCOUNT_ID:thing/esp32timer-*"]
    }
  ]
}
```

Replace REGION and ACCOUNT_ID, then:
```bash
aws iot create-policy --policy-name esp32timer-policy --policy-document file://policy.json
aws iot attach-policy --policy-name esp32timer-policy --target CERTIFICATE_ARN
aws iot attach-thing-principal --thing-name esp32timer-f76c5c --principal CERTIFICATE_ARN
```

### Step 4: Provision Device (Development Method)

Create a file `provision_certs.h` in the `main/` directory:

```c
#ifndef PROVISION_CERTS_H
#define PROVISION_CERTS_H

// Paste your device certificate here
const char *DEVICE_CERT = 
"-----BEGIN CERTIFICATE-----\n"
"MIIDWTCCAkGgAwIBAgIUXXXXXXXXXXXXXXXXXXXXXXXXXXX...\n"
// ... paste full certificate ...
"-----END CERTIFICATE-----\n";

// Paste your private key here
const char *PRIVATE_KEY = 
"-----BEGIN RSA PRIVATE KEY-----\n"
"MIIEpAIBAAKCAQEAXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX...\n"
// ... paste full private key ...
"-----END RSA PRIVATE KEY-----\n";

// Paste AWS Root CA here
const char *ROOT_CA = 
"-----BEGIN CERTIFICATE-----\n"
"MIIDQTCCAimgAwIBAgITBmyfz5m/jAo54vB4ikPmljZbyjANBgkqhkiG9w0BAQsF\n"
// ... paste full root CA ...
"-----END CERTIFICATE-----\n";

#endif
```

Add to `main.c` after includes:
```c
#include "provision_certs.h"

// Add this function before app_main()
static void provision_iot_credentials_once(void)
{
    // Check if already provisioned
    if (cert_store_has_credentials()) {
        ESP_LOGI(TAG, "Certificates already provisioned");
        return;
    }
    
    ESP_LOGI(TAG, "Provisioning IoT Core certificates...");
    esp_err_t ret = cert_store_set_credentials(DEVICE_CERT, PRIVATE_KEY, ROOT_CA);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "✓ Credentials provisioned successfully!");
    } else {
        ESP_LOGE(TAG, "✗ Failed to provision credentials");
    }
}
```

Call it in `app_main()` after NVS init:
```c
// Provision certificates (only runs once)
provision_iot_credentials_once();

// Initialize IoT Core
ESP_LOGI(TAG, "Initializing IoT Core manager...");
esp_err_t iot_ret = iot_manager_init();
if (iot_ret == ESP_OK) {
    ESP_LOGI(TAG, "IoT Core manager initialized successfully");
    
    // Try a sync session (for testing)
    ESP_LOGI(TAG, "Testing IoT Core sync session...");
    iot_ret = iot_manager_sync_session();
    if (iot_ret == ESP_OK) {
        ESP_LOGI(TAG, "✓ IoT Core sync successful!");
    } else {
        ESP_LOGE(TAG, "✗ IoT Core sync failed");
    }
} else {
    ESP_LOGI(TAG, "IoT Core not available");
}
```

### Step 5: Build and Test

```powershell
.\build.ps1
.\deploy.ps1
```

### Expected Output (Success)

```
I (xxxx) cert_store: Credentials check: cert=1, key=1, ca=1
I (xxxx) iot_manager: Device ID: esp32timer-f76c5c
I (xxxx) iot_manager: IoT Core manager initialized successfully
I (xxxx) main: Testing IoT Core sync session...
I (xxxx) iot_manager: Starting IoT Core sync session
I (xxxx) iot_manager: MQTT connected successfully
I (xxxx) iot_manager: Published connection status
I (xxxx) iot_manager: Shadow update published successfully
I (xxxx) iot_manager: IoT Core sync session completed successfully
I (xxxx) main: ✓ IoT Core sync successful!
```

### Step 6: Verify in AWS Console

1. Go to AWS IoT Core Console → Test → MQTT test client
2. Subscribe to `esp32timer/+/status/#`
3. You should see messages from your device

## Troubleshooting

### "MQTT connection timeout"
- Check endpoint in config.h matches AWS
- Verify certificates are correct
- Check WiFi connectivity
- Verify policy is attached to certificate

### "Failed to load device certificate"
- Certificates not provisioned correctly
- Check serial output for cert_store errors

### "Certificate validation failure"
- Wrong Root CA certificate
- Check you downloaded AmazonRootCA1.pem correctly

### Device still in BLE mode
- This is normal! The device falls back to BLE if IoT Core fails
- Check logs for the specific error
- Device will retry IoT Core every hour

## Security Note

**IMPORTANT:** After testing, remove `provision_certs.h` from your repository:
```bash
git rm main/provision_certs.h
echo "main/provision_certs.h" >> .gitignore
```

Never commit certificates to version control!

## Next Steps

Once IoT Core is working:
1. Implement full dual-mode operation (Task 6-8)
2. Add connection manager for automatic mode switching
3. Integrate into boot sequence with proper timing
4. Test web client integration

For production, implement a secure provisioning process instead of hardcoding certificates.
