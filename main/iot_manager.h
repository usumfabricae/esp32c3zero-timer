#ifndef IOT_MANAGER_H
#define IOT_MANAGER_H

#include "esp_err.h"
#include <stdbool.h>

/**
 * @brief Initialize IoT Core manager
 * 
 * Loads credentials from cert_store and initializes MQTT client
 * 
 * @return ESP_OK on success, ESP_FAIL if credentials missing or initialization fails
 */
esp_err_t iot_manager_init(void);

/**
 * @brief Device state structure for shadow synchronization
 */
typedef struct {
    uint8_t battery;              // Battery percentage (0-100)
    float temperature;            // Temperature in Celsius
    char relay_state;             // Current relay state ('H', 'L', 'O')
    int64_t timestamp;            // Unix timestamp
    char schedule_mode;           // Current schedule mode ('H', 'L', 'O')
    struct {
        float high;               // High temperature threshold
        float low;                // Low temperature threshold
    } thresholds;
    char schedule[7][25];         // Weekly schedule (7 days × 24 hours + null terminator)
    struct {
        int16_t temp_points[10][2];    // Temperature calibration points [voltage_mv, temp_celsius]
        int16_t battery_points[10][2]; // Battery calibration points [adc_raw, percentage]
        uint8_t temp_count;            // Number of temperature calibration points
        uint8_t battery_count;         // Number of battery calibration points
    } calibration;
} device_state_t;

/**
 * @brief Publish device state to Device Shadow reported section
 * 
 * Publishes current device state to AWS IoT Device Shadow.
 * Must be called during an active MQTT connection.
 * 
 * @param state Pointer to device state structure
 * @return ESP_OK on success, ESP_FAIL on error
 */
esp_err_t iot_manager_publish_state(const device_state_t *state);

/**
 * @brief Publish status message to telemetry topic
 * 
 * Publishes device telemetry to esp32timer/{deviceId}/status/telemetry topic.
 * Must be called during an active MQTT connection.
 * Completes within 2 seconds to minimize WiFi power consumption.
 * 
 * @param state Pointer to device state structure
 * @return ESP_OK on success, ESP_FAIL on error
 */
esp_err_t iot_manager_publish_status_message(const device_state_t *state);

/**
 * @brief Process Device Shadow delta updates
 * 
 * Subscribes to shadow delta topic and processes desired state changes.
 * Applies changes to device configuration and publishes confirmation.
 * Must be called during an active MQTT connection.
 * 
 * @return ESP_OK on success, ESP_FAIL on error
 */
esp_err_t iot_manager_process_delta(void);

/**
 * @brief Perform brief synchronization session with IoT Core
 * 
 * Connects to AWS IoT Core via MQTT, publishes device state to shadow,
 * receives and processes pending commands, then disconnects.
 * 
 * This function implements the disconnected periodic sync pattern:
 * - Connect MQTT with TLS 1.2 (10-second timeout)
 * - Subscribe to command topic
 * - Publish connection status
 * - Publish device state to shadow
 * - Process shadow delta updates
 * - Disconnect immediately
 * 
 * Total duration: 5-30 seconds (variable based on operations)
 * 
 * @return ESP_OK if sync successful, ESP_FAIL if should fallback to BLE mode
 */
esp_err_t iot_manager_sync_session(void);

/**
 * @brief Check if IoT Core is marked as reachable
 * 
 * @return true if IoT Core is reachable, false if in BLE fallback mode
 */
bool iot_manager_is_reachable(void);

/**
 * @brief Mark IoT Core as unreachable (triggers BLE mode)
 * 
 * Stores unreachable state in RTC memory and NVS for persistence
 */
void iot_manager_mark_unreachable(void);

/**
 * @brief Cleanup and disconnect IoT Core manager
 * 
 * Disconnects MQTT client and frees resources
 */
void iot_manager_deinit(void);

#endif // IOT_MANAGER_H
