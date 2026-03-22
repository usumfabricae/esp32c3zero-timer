# Next Steps: AWS IoT Core Integration

## Current Status

✅ IoT Core code compiled and flashed successfully
✅ Certificates provisioned to device NVS
✅ IoT endpoint configured in `main/config.h`
✅ Device attempting IoT Core connection (getting connection refused - expected!)

## What's Happening Now

The device is successfully:
1. ✅ Loading certificates from NVS
2. ✅ Initializing MQTT client with TLS 1.2
3. ✅ Attempting connection to AWS IoT Core endpoint
4. ❌ Getting connection refused (errno=119 ECONNRESET)
## AWS Setup Required (Do This Now!)

The device is ready and trying to connect. Complete these AWS setup steps to allow the connection:
**This is expected!** AWS is refusing the connection because we haven't set up the IoT thing and policy yet.

## AWS Setup Required

Before the device can successfully connect, you need to complete AWS setup:

### 1. Get Your IoT Endpoint (Already Done)
Your endpoint is already configured in `config.h`:
```
azjenkdbqqlx7-ats.iot.eu-central-1.amazonaws.com
```

### 2. Create IoT Thing in AWS

Get your device ID from the serial monitor (it will show on boot), then:

```bash
# Replace esp32timer-f76c5c with your actual device ID from logs
aws iot create-thing --thing-name esp32timer-f76c5c
```

### 3. Create and Attach IoT Policy

Create a file `esp32timer-policy.json`:

```json
{
  "Version": "2012-10-17",
  "Statement": [
    {
      "Effect": "Allow",
      "Action": [
        "iot:Connect"
      ],
      "Resource": "arn:aws:iot:eu-central-1:348831852500:client/esp32timer-*"
    },
    {
      "Effect": "Allow",
      "Action": [
        "iot:Publish"
      ],
      "Resource": [
        "arn:aws:iot:eu-central-1:348831852500:topic/esp32timer/*/status/*",
        "arn:aws:iot:eu-central-1:348831852500:topic/$aws/things/esp32timer-*/shadow/update"
      ]
    },
    {
      "Effect": "Allow",
      "Action": [
        "iot:Subscribe"
      ],
      "Resource": [
        "arn:aws:iot:eu-central-1:348831852500:topicfilter/esp32timer/*/commands/#",
        "arn:aws:iot:eu-central-1:348831852500:topicfilter/$aws/things/esp32timer-*/shadow/update/delta"
      ]
    },
    {
      "Effect": "Allow",
      "Action": [
        "iot:Receive"
      ],
      "Resource": [
        "arn:aws:iot:eu-central-1:348831852500:topic/esp32timer/*/commands/#",
        "arn:aws:iot:eu-central-1:348831852500:topic/$aws/things/esp32timer-*/shadow/update/delta"
      ]
    }
  ]
}
```

Create the policy:

```bash
aws iot create-policy --policy-name esp32timer-policy --policy-document file://esp32timer-policy.json
```

### 4. Attach Policy to Certificate

```bash
# Your certificate ARN
CERT_ARN="arn:aws:iot:eu-central-1:348831852500:cert/711bdf0bd55393d90d15aaeee149d4c4aeea6f0b95ad66e387f8f34fbad50cd5"

# Attach policy
aws iot attach-policy --policy-name esp32timer-policy --target $CERT_ARN
```

### 5. Attach Certificate to Thing

```bash
# Replace with your device ID from logs
aws iot attach-thing-principal --thing-name esp32timer --principal $CERT_ARN
```

## Flash and Test

1. Flash the device:
   ```powershell
   .\deploy.ps1
   ```

2. Watch the serial output for:
## Current Log Output (Before AWS Setup)

```
I (5925) cert_store: Device certificate retrieved (1221 bytes)
I (5935) cert_store: Private key retrieved (1680 bytes)
I (5935) cert_store: Root CA retrieved (1189 bytes)
I (5945) iot_manager: Certificates loaded successfully
I (5945) iot_manager: IoT Core manager initialized successfully
I (5955) main: ✓ IoT Core manager initialized successfully
I (5955) main: Testing IoT Core sync session...
E (7635) mqtt_client: esp_mqtt_handle_transport_read_error: transport_read(): EOF
E (7635) mqtt_client: esp_mqtt_handle_transport_read_error: transport_read() error: errno=119
E (7645) iot_manager: MQTT error occurred
E (7645) iot_manager: TCP transport error
E (17065) iot_manager: MQTT connection timeout after 10 seconds
W (17665) iot_manager: Marking IoT Core as unreachable, switching to BLE mode
E (17665) main: ✗ IoT Core sync failed: ESP_FAIL
```

**This is expected!** The connection is being refused because AWS doesn't recognize the certificate yet.

## Expected Log Output (After AWS Setup)

```
I (xxx) iot_manager: MQTT connected to IoT Core
I (xxx) iot_manager: Subscribed to command topic: esp32timer/esp32timer-f76c5c/commands/#
I (xxx) iot_manager: Published connection status
I (xxx) iot_manager: Shadow update published successfully
I (xxx) iot_manager: No delta updates pending
I (xxx) iot_manager: IoT Core sync session completed successfully
I (xxx) main: ✓ IoT Core sync successful!
```xxx) iot_manager: Connecting to AWS IoT Core...
I (xxx) iot_manager: ✓ Connected to AWS IoT Core
I (xxx) iot_manager: Publishing device state...
I (xxx) iot_manager: ✓ Device state published
I (xxx) main: ✓ IoT Core sync successful!
```

## Verify in AWS Console

1. Go to AWS IoT Core console
2. Navigate to "Test" → "MQTT test client"
3. Subscribe to: `esp32timer/+/status/#`
4. You should see status messages from your device

## Troubleshooting

### Connection Timeout
- Check WiFi credentials in `config.h`
- Verify IoT endpoint is correct
- Check AWS policy is attached to certificate

### Certificate Error
- Verify certificate ARN matches in AWS
- Check certificate is attached to thing
- Verify policy allows connection

### Policy Error
- Check policy JSON is valid
- Verify policy is attached to certificate
- Check resource ARNs match your region and account

## Security Note

After successful provisioning, you should:

1. Remove `main/provision_certs.h` from your workspace
2. Add it to `.gitignore` to prevent accidental commits
3. For production, use a secure provisioning process (not hardcoded certificates)

## Next Phase

Once IoT Core connection is working:
- Device will sync every 5 minutes in IoT mode
- Falls back to BLE mode if IoT unreachable
- Retries IoT connection every hour in BLE mode
- Web client integration (Phase 2) will follow
