#ifndef CONFIG_H
#define CONFIG_H

/**
 * ============================================================================
 * ESP32-C3 Timer Configuration File
 * ============================================================================
 * This file contains all user-configurable parameters for the device.
 * Modify these values to customize the device behavior.
 */

// ============================================================================
// DEVICE IDENTIFICATION
// ============================================================================

/**
 * Device name used for:
 * - BLE advertising name
 * - WiFi hostname
 * - Device identification in web interface
 * Maximum length: 20 characters
 */
#define DEVICE_NAME "ESP32C3_Timer"

// ============================================================================
// WIFI CONFIGURATION
// ============================================================================

/**
 * WiFi network SSID (network name)
 * The device will connect to this WiFi network for NTP time synchronization
 */
#define WIFI_SSID "NETNIAERO"

/**
 * WiFi network password
 * Leave empty "" for open networks (not recommended)
 */
#define WIFI_PASS "t3s0r1n0b3ll0bello"

/**
 * Maximum number of WiFi connection retry attempts
 * If connection fails after this many attempts, the device will continue without WiFi
 */
#define WIFI_MAXIMUM_RETRY 5

// ============================================================================
// POWER MANAGEMENT
// ============================================================================

/**
 * Deep sleep duration in seconds
 * The device will wake up periodically to check schedule and update relay
 * Recommended: 30-60 seconds for normal operation
 * Lower values = more responsive but higher power consumption
 * Higher values = lower power consumption but less responsive
 */
#define DEEP_SLEEP_DURATION_SEC 30

/**
 * BLE advertising timeout in seconds
 * How long the device will advertise and wait for BLE connection before going to sleep
 * Recommended: 10-30 seconds
 */
#define BLE_ADVERTISING_TIMEOUT_SEC 10

// ============================================================================
// TIME SYNCHRONIZATION
// ============================================================================

/**
 * NTP resync interval in seconds
 * How often the device will reconnect to WiFi to resynchronize time with NTP server
 * Recommended: 6 hours (21600 seconds) to 24 hours (86400 seconds)
 * The ESP32-C3 RTC maintains accurate time between syncs
 */
#define NTP_RESYNC_INTERVAL_SEC (6 * 60 * 60)  // 6 hours

/**
 * Timezone configuration
 * Format: "TZ-offset[DST-name,DST-start,DST-end]"
 * Default: "CET-1CEST,M3.5.0,M10.5.0/3" (Central European Time with DST)
 * Examples:
 * - EST (US Eastern): "EST5EDT,M3.2.0/2,M11.1.0/2"
 * - PST (US Pacific): "PST8PDT,M3.2.0/2,M11.1.0/2"
 * - JST (Japan): "JST-9"
 * - UTC: "UTC0"
 */
#define TIMEZONE_CONFIG "CET-1CEST,M3.5.0,M10.5.0/3"

// ============================================================================
// RELAY CONTROL
// ============================================================================

/**
 * Relay hysteresis time in seconds
 * Minimum time between automatic relay state changes to prevent rapid switching
 * This protects the relay and connected equipment from frequent on/off cycles
 * Recommended: 300 seconds (5 minutes)
 */
#define RELAY_HYSTERESIS_SEC (5 * 60)  // 5 minutes

/**
 * Manual override duration in seconds
 * When relay is manually controlled via BLE, automatic scheduling is suspended for this duration
 * After this time expires, automatic scheduling resumes
 * Recommended: 3600 seconds (1 hour)
 */
#define MANUAL_OVERRIDE_DURATION_SEC (60 * 60)  // 1 hour

/**
 * Bistable relay pulse duration in milliseconds
 * How long to apply voltage to switch the bistable relay
 * Recommended: 500-1000 ms depending on relay specifications
 * Too short: relay may not switch reliably
 * Too long: unnecessary power consumption
 */
#define RELAY_PULSE_DURATION_MS 500

// ============================================================================
// TEMPERATURE SENSOR CALIBRATION
// ============================================================================

/**
 * Default temperature calibration point 1
 * Voltage reading (in millivolts) at 20°C
 * Used when no custom calibration is available
 * Adjust based on your specific temperature sensor characteristics
 */
#define DEFAULT_TEMP_20C_MV 1471

/**
 * Default temperature calibration point 2
 * Voltage reading (in millivolts) at 30°C
 * Used when no custom calibration is available
 * Adjust based on your specific temperature sensor characteristics
 */
#define DEFAULT_TEMP_30C_MV 1266

/**
 * Maximum number of temperature calibration points
 * The device uses linear regression with multiple calibration points for accuracy
 * More points = better accuracy across wider temperature range
 * Recommended: 5-10 points
 */
#define MAX_CALIBRATION_POINTS 10

// ============================================================================
// BATTERY MONITORING
// ============================================================================

/**
 * Battery voltage at 100% charge (in millivolts)
 * For Li-ion batteries: typically 4200 mV (4.2V)
 * For LiPo batteries: typically 4200 mV (4.2V)
 * For other battery types: adjust accordingly
 */
#define BATTERY_MAX_MV 4200

/**
 * Battery voltage at 0% charge (in millivolts)
 * For Li-ion/LiPo batteries: typically 3000 mV (3.0V)
 * Warning: Discharging below this voltage may damage the battery
 */
#define BATTERY_MIN_MV 3000

/**
 * Low battery warning threshold (percentage)
 * When battery level drops below this percentage, LED shows yellow warning
 * Recommended: 20-30%
 */
#define BATTERY_LOW_THRESHOLD_PERCENT 30

// ============================================================================
// BLE SECURITY
// ============================================================================

/**
 * Default BLE pairing passkey
 * 6-digit numeric code displayed during device pairing
 * The device will show this passkey in the serial monitor during pairing
 * User must enter this passkey on their phone/laptop to complete pairing
 * Can be changed via BLE after first pairing
 * Range: 000000 to 999999
 * 
 * IMPORTANT: This passkey will be displayed in the serial monitor when
 * a device attempts to pair. Monitor the serial output to see the passkey.
 */
#define DEFAULT_PASSKEY 123456

// ============================================================================
// GPIO PIN ASSIGNMENTS
// ============================================================================

/**
 * Relay control pins (H-bridge configuration for bistable relay)
 * GPIO6 and GPIO7: Direction A (relay ON)
 * GPIO4 and GPIO5: Direction B (relay OFF)
 * Note: These are parallel pairs for higher current capacity
 */
#define RELAY_GPIO_1A 6
#define RELAY_GPIO_1B 7
#define RELAY_GPIO_2A 4
#define RELAY_GPIO_2B 5

/**
 * Temperature sensor ADC pin
 * Connected to analog temperature sensor
 */
#define TEMP_SENSOR_GPIO 3

/**
 * Battery voltage monitor ADC pin
 * Connected to battery voltage divider (reads 1/2 of actual battery voltage)
 */
#define BATTERY_VOLTAGE_GPIO 2

/**
 * Status LED pin (WS2812 RGB LED)
 * Used to indicate device status:
 * - Red: Syncing time with NTP
 * - Green: Time synced, normal operation
 * - Blue: BLE client connected
 * - Yellow: Low battery warning
 */
#define STATUS_LED_GPIO 10

// ============================================================================
// LED STATUS BRIGHTNESS
// ============================================================================

/**
 * LED brightness level (0-255)
 * Lower values = dimmer LED, less power consumption
 * Higher values = brighter LED, more visible
 * Recommended: 10-50 for indoor use
 */
#define LED_BRIGHTNESS 10

#endif // CONFIG_H
