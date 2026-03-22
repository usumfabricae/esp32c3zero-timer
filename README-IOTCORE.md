# AWS IoT Core Setup Guide

This guide provides step-by-step instructions for setting up AWS IoT Core integration with your ESP32-C3 timer device.

## Overview

The device uses AWS IoT Core for cloud connectivity with the following features:
- Disconnected periodic sync pattern (connect every 5 minutes, sync, disconnect)
- Device Shadow for state synchronization
- TLS 1.2 mutual authentication with X.509 certificates
- Automatic fallback to BLE mode if IoT Core is unreachable
- Power-optimized: WiFi active only during brief sync sessions (5-30 seconds)

## Prerequisites

- AWS Account with IoT Core access
- AWS CLI installed and configured ([Installation Guide](https://docs.aws.amazon.com/cli/latest/userguide/getting-started-install.html))
- ESP32-C3 device with firmware built and ready to flash
- Serial connection to device for provisioning

## Step 1: Configure IoT Core Endpoint in Firmware

Before creating the IoT thing, you need to find your AWS IoT Core endpoint and configure it in the firmware.

### 1.1 Get Your IoT Core Endpoint

```bash
aws iot describe-endpoint --endpoint-type iot:Data-ATS
```

This will return something like:
```json
{
    "endpointAddress": "a1b2c3d4e5f6g7-ats.iot.us-east-1.amazonaws.com"
}
```

### 1.2 Update config.h

Edit `main/config.h` and replace the placeholder endpoint:

```c
// Replace this line:
#define IOT_ENDPOINT "your-endpoint-ats.iot.region.amazonaws.com"

// With your actual endpoint:
#define IOT_ENDPOINT "a1b2c3d4e5f6g7-ats.iot.us-east-1.amazonaws.com"
```

### 1.3 Rebuild Firmware

```powershell
.\build.ps1
```

## Step 2: Create IoT Thing and Generate Certificates

### 2.1 Determine Your Device ID

The device generates its ID from the MAC address. You can either:
- Flash the device and check the serial output for "Device ID: esp32timer-XXXXXX"
- Or use a placeholder and update later

For this guide, we'll use `esp32timer-abc123` as an example. Replace with your actual device ID.

### 2.2 Create the IoT Thing

```bash
aws iot create-thing --thing-name esp32timer-abc123
```

Expected output:
```json
{
    "thingName": "esp32timer-abc123",
    "thingArn": "arn:aws:iot:us-east-1:123456789012:thing/esp32timer-abc123",
    "thingId": "12345678-1234-1234-1234-123456789012"
}
```

### 2.3 Generate Device Certificates

```bash
aws iot create-keys-and-certificate \
    --set-as-active \
    --certificate-pem-outfile esp32timer-abc123.cert.pem \
    --public-key-outfile esp32timer-abc123.public.key \
    --private-key-outfile esp32timer-abc123.private.key
```

**IMPORTANT:** Save the output! You'll need the `certificateArn` for the next step.

Expected output:
```json
{
    "certificateArn": "arn:aws:iot:us-east-1:123456789012:cert/abcdef1234567890...",
    "certificateId": "abcdef1234567890...",
    "certificatePem": "-----BEGIN CERTIFICATE-----\n...",
    "keyPair": {
        "PublicKey": "-----BEGIN PUBLIC KEY-----\n...",
        "PrivateKey": "-----BEGIN RSA PRIVATE KEY-----\n..."
    }
}
```

### 2.4 Download AWS Root CA Certificate

```bash
curl -o AmazonRootCA1.pem https://www.amazontrust.com/repository/AmazonRootCA1.pem
```

You should now have these files:
- `esp32timer-abc123.cert.pem` - Device certificate
- `esp32timer-abc123.private.key` - Private key
- `AmazonRootCA1.pem` - AWS root CA certificate

## Step 3: Create and Attach IoT Policy

### 3.1 Create Policy JSON File

Create a file named `esp32timer-policy.json` with the following content:

```json
{
  "Version": "2012-10-17",
  "Statement": [
    {
      "Effect": "Allow",
      "Action": [
        "iot:Connect"
      ],
      "Resource": [
        "arn:aws:iot:us-east-1:YOUR_ACCOUNT_ID:client/esp32timer-*"
      ]
    },
    {
      "Effect": "Allow",
      "Action": [
        "iot:Publish"
      ],
      "Resource": [
        "arn:aws:iot:us-east-1:YOUR_ACCOUNT_ID:topic/esp32timer/*/status/*",
        "arn:aws:iot:us-east-1:YOUR_ACCOUNT_ID:topic/$aws/things/esp32timer-*/shadow/update"
      ]
    },
    {
      "Effect": "Allow",
      "Action": [
        "iot:Subscribe"
      ],
      "Resource": [
        "arn:aws:iot:us-east-1:YOUR_ACCOUNT_ID:topicfilter/esp32timer/*/commands/#",
        "arn:aws:iot:us-east-1:YOUR_ACCOUNT_ID:topicfilter/$aws/things/esp32timer-*/shadow/update/delta"
      ]
    },
    {
      "Effect": "Allow",
      "Action": [
        "iot:Receive"
      ],
      "Resource": [
        "arn:aws:iot:us-east-1:YOUR_ACCOUNT_ID:topic/esp32timer/*/commands/*",
        "arn:aws:iot:us-east-1:YOUR_ACCOUNT_ID:topic/$aws/things/esp32timer-*/shadow/update/delta"
      ]
    },
    {
      "Effect": "Allow",
      "Action": [
        "iot:GetThingShadow",
        "iot:UpdateThingShadow"
      ],
      "Resource": [
        "arn:aws:iot:us-east-1:YOUR_ACCOUNT_ID:thing/esp32timer-*"
      ]
    }
  ]
}
```

**Replace `YOUR_ACCOUNT_ID`** with your AWS account ID. You can find it with:
```bash
aws sts get-caller-identity --query Account --output text
```

### 3.2 Create the Policy

```bash
aws iot create-policy \
    --policy-name esp32timer-policy \
    --policy-document file://esp32timer-policy.json
```

### 3.3 Attach Policy to Certificate

```bash
aws iot attach-policy \
    --policy-name esp32timer-policy \
    --target "CERTIFICATE_ARN_FROM_STEP_2.3"
```

### 3.4 Attach Certificate to Thing

```bash
aws iot attach-thing-principal \
    --thing-name esp32timer-abc123 \
    --principal "CERTIFICATE_ARN_FROM_STEP_2.3"
```

## Step 4: Provision Device with Certificates

Now you need to load the certificates into the device's NVS storage. There are two methods:

### Method A: Via Serial Console (Recommended for Development)

This method requires adding a provisioning command to the firmware temporarily.

1. Add a simple provisioning function to `main.c` (temporary, for development only):

```c
// Add this function before app_main()
static void provision_iot_credentials(void)
{
    // Read certificates from files (you'll need to paste them as strings)
    const char *device_cert = 
        "-----BEGIN CERTIFICATE-----\n"
        "MIIDWTCCAkGgAwIBAgIUXXXXXXXXXXXXXXXXXXXXXXXXXXX...\n"
        // ... paste full certificate here ...
        "-----END CERTIFICATE-----\n";
    
    const char *private_key = 
        "-----BEGIN RSA PRIVATE KEY-----\n"
        "MIIEpAIBAAKCAQEAXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX...\n"
        // ... paste full private key here ...
        "-----END RSA PRIVATE KEY-----\n";
    
    const char *root_ca = 
        "-----BEGIN CERTIFICATE-----\n"
        "MIIDQTCCAimgAwIBAgITBmyfz5m/jAo54vB4ikPmljZbyjANBgkqhkiG9w0BAQsF...\n"
        // ... paste full root CA here ...
        "-----END CERTIFICATE-----\n";
    
    esp_err_t ret = cert_store_set_credentials(device_cert, private_key, root_ca);
    if (ret == ESP_OK) {
        ESP_LOGI("provision", "Credentials stored successfully!");
    } else {
        ESP_LOGE("provision", "Failed to store credentials");
    }
}
```

2. Call this function once in `app_main()`:
```c
void app_main(void)
{
    // ... existing initialization code ...
    
    // Provision credentials (comment out after first run)
    provision_iot_credentials();
    
    // ... rest of app_main ...
}
```

3. Flash the device:
```powershell
.\deploy.ps1
```

4. Check serial output for "Credentials stored successfully!"

5. **Important:** Comment out the `provision_iot_credentials()` call and rebuild/reflash to remove the hardcoded certificates from the firmware.

### Method B: Via Custom Provisioning Tool (Production)

For production, you should create a separate provisioning tool that:
1. Connects to the device via serial
2. Sends certificates over a secure channel
3. Device stores them in NVS

This is more secure as certificates are never compiled into the firmware.

## Step 5: Verify Configuration

### 5.1 Check Device Logs

After flashing, monitor the serial output:

```powershell
.\trace.ps1
```

Look for these log messages:
```
I (1234) cert_store: Credentials check: cert=1, key=1, ca=1
I (1235) iot_manager: Device ID: esp32timer-abc123
I (1236) iot_manager: IoT Core manager initialized successfully
I (1237) iot_manager: Starting IoT Core sync session
I (1238) iot_manager: MQTT connected successfully
I (1239) iot_manager: Published connection status
I (1240) iot_manager: IoT Core sync session completed successfully
```

### 5.2 Monitor AWS IoT Core

In the AWS IoT Core console:

1. Go to **Test** → **MQTT test client**
2. Subscribe to `esp32timer/+/status/#`
3. You should see connection and telemetry messages every 5 minutes

### 5.3 Check Device Shadow

1. Go to **Manage** → **All devices** → **Things**
2. Click on your thing (`esp32timer-abc123`)
3. Go to **Device Shadows** → **Classic Shadow**
4. You should see the reported state with battery, temperature, relay state, etc.

## MQTT Topic Structure

The device uses the following topic pattern: `esp32timer/{deviceId}/{message_type}`

### Published Topics (Device → Cloud)

| Topic | Purpose | Frequency |
|-------|---------|-----------|
| `esp32timer/{deviceId}/status/connection` | Connection status | Every sync session |
| `esp32timer/{deviceId}/status/telemetry` | Device telemetry | Every 5 min (IoT mode) or 60 min (BLE mode) |
| `$aws/things/{deviceId}/shadow/update` | Shadow updates | Every sync session |

### Subscribed Topics (Cloud → Device)

| Topic | Purpose |
|-------|---------|
| `esp32timer/{deviceId}/commands/#` | Direct commands (legacy) |
| `$aws/things/{deviceId}/shadow/update/delta` | Shadow delta updates |

## Message Formats

### Connection Status Message

```json
{
  "status": "connected",
  "timestamp": 1704067200
}
```

### Telemetry Message

```json
{
  "version": "1.0",
  "device_id": "esp32timer-abc123",
  "timestamp": 1704067200,
  "battery": 85,
  "temperature": 21.5,
  "relay_state": "H",
  "schedule_mode": "H",
  "wifi_rssi": -65,
  "uptime_seconds": 3600
}
```

### Device Shadow Structure

```json
{
  "state": {
    "reported": {
      "battery": 85,
      "temperature": 21.5,
      "relay_state": "H",
      "timestamp": 1704067200,
      "schedule_mode": "H",
      "thresholds": {
        "high": 22.0,
        "low": 18.0
      },
      "schedule": {
        "monday": "HHHHHHLLLLLLLLLLLLLHHHHH",
        "tuesday": "HHHHHHLLLLLLLLLLLLLHHHHH",
        "wednesday": "HHHHHHLLLLLLLLLLLLLHHHHH",
        "thursday": "HHHHHHLLLLLLLLLLLLLHHHHH",
        "friday": "HHHHHHLLLLLLLLLLLLLHHHHH",
        "saturday": "OOOOOOOOOOOOOOOOOOOOOOOO",
        "sunday": "OOOOOOOOOOOOOOOOOOOOOOOO"
      },
      "calibration": {
        "temp_points": [[0, 0], [1024, 100]],
        "battery_points": [[0, 0], [4095, 100]]
      }
    },
    "desired": {
      "override": {
        "active": true,
        "duration_minutes": 60
      },
      "thresholds": {
        "high": 23.0,
        "low": 19.0
      }
    }
  }
}
```

## Sending Commands to Device

### Via Device Shadow (Recommended)

Update the desired state in the shadow:

```bash
aws iot-data update-thing-shadow \
    --thing-name esp32timer-abc123 \
    --payload '{"state":{"desired":{"override":{"active":true,"duration_minutes":60}}}}' \
    /dev/stdout
```

The device will receive the delta on its next sync session (within 5 minutes) and apply the changes.

### Via Direct MQTT (Legacy)

Publish to the commands topic:

```bash
aws iot-data publish \
    --topic esp32timer/esp32timer-abc123/commands/override \
    --payload '{"command":"override","duration_minutes":60}'
```

## Troubleshooting

### Device Not Connecting

**Check 1: Credentials**
```
I (1234) cert_store: Credentials check: cert=0, key=0, ca=0
I (1235) iot_manager: IoT Core credentials not found, operating in BLE-only mode
```
→ Certificates not provisioned. Repeat Step 4.

**Check 2: Endpoint Configuration**
```
E (1234) iot_manager: Failed to start MQTT client
```
→ Check `IOT_ENDPOINT` in `config.h` matches your AWS endpoint.

**Check 3: Certificate/Policy Issues**
```
E (1234) iot_manager: MQTT connection timeout after 10 seconds
```
→ Verify certificate is attached to thing and policy is attached to certificate.

**Check 4: Network Connectivity**
```
E (1234) wifi_manager: WiFi connection failed
```
→ Check WiFi credentials in `config.h`.

### Device Connects But No Shadow Updates

**Check AWS IoT Core Logs:**
1. Go to **Settings** → **Logs**
2. Enable logging if not already enabled
3. Check CloudWatch Logs for errors

**Common Issues:**
- Policy doesn't allow shadow operations
- Thing name mismatch between device ID and AWS thing name
- Shadow topic format incorrect

### Device Falls Back to BLE Mode

This is normal behavior when IoT Core is unreachable. The device will:
1. Retry IoT Core connection once per hour
2. Operate in BLE mode (1-minute wake cycles) in the meantime
3. Automatically switch back to IoT mode when connection succeeds

Check logs for the reason:
```
W (1234) iot_manager: Marking IoT Core as unreachable, switching to BLE mode
```

## Security Best Practices

1. **Never commit certificates to version control**
   - Add `*.pem`, `*.key`, `*.cert` to `.gitignore`

2. **Use unique certificates per device**
   - Generate new certificates for each device
   - Never reuse certificates across devices

3. **Rotate certificates regularly**
   - AWS IoT Core supports certificate rotation
   - Plan for certificate expiration (default: 10 years)

4. **Restrict policy permissions**
   - Use least-privilege principle
   - Limit topics to device-specific patterns

5. **Enable flash encryption**
   - ESP32-C3 supports flash encryption
   - Protects private keys stored in NVS

6. **Monitor certificate usage**
   - Use AWS IoT Core metrics to detect anomalies
   - Set up CloudWatch alarms for unusual activity

## Next Steps

- **Web Client Integration**: See Task 13-18 in the implementation plan for web client setup
- **IAM Credentials**: Configure IAM user for web client access to IoT Core
- **Testing**: Use AWS IoT Core MQTT test client to verify bidirectional communication
- **Production**: Implement secure provisioning process for manufacturing

## Support

For issues specific to:
- **AWS IoT Core**: [AWS IoT Core Documentation](https://docs.aws.amazon.com/iot/)
- **ESP-IDF**: [ESP-IDF Documentation](https://docs.espressif.com/projects/esp-idf/)
- **This Project**: Check the main README.md and open an issue on GitHub
