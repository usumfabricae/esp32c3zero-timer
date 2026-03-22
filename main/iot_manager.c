#include "iot_manager.h"
#include "cert_store.h"
#include "config.h"
#include "scheduler.h"
#include "gpio_manager.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_wifi.h"
#include "mqtt_client.h"
#include "esp_tls.h"
#include "nvs_flash.h"
#include "cJSON.h"
#include <string.h>
#include <time.h>

static const char *TAG = "iot_manager";

// MQTT client handle
static esp_mqtt_client_handle_t mqtt_client = NULL;

// Connection state
static bool mqtt_connected = false;
static bool sync_in_progress = false;

// Reachability state (persisted in RTC memory and NVS)
static RTC_DATA_ATTR bool iot_reachable = true;

// NVS namespace for IoT state
#define IOT_STATE_NAMESPACE "iot_state"
#define IOT_REACHABLE_KEY "reachable"

// Device ID (derived from MAC address)
static char device_id[32] = {0};

// MQTT topics
static char topic_commands[128] = {0};
static char topic_status_connection[128] = {0};
static char topic_status_telemetry[128] = {0};
static char topic_shadow_update[128] = {0};
static char topic_shadow_delta[128] = {0};

// Delta processing state
static bool delta_received = false;
static char delta_payload[4096] = {0};
static size_t delta_payload_len = 0;

/**
 * @brief Generate device ID from MAC address
 */
static void generate_device_id(void)
{
    uint8_t mac[6];
    esp_efuse_mac_get_default(mac);
    snprintf(device_id, sizeof(device_id), "esp32timer-%02x%02x%02x",
             mac[3], mac[4], mac[5]);
    ESP_LOGI(TAG, "Device ID: %s", device_id);
}

/**
 * @brief Generate MQTT topics based on device ID
 */
static void generate_mqtt_topics(void)
{
    snprintf(topic_commands, sizeof(topic_commands),
             "esp32timer/%s/commands/#", device_id);
    snprintf(topic_status_connection, sizeof(topic_status_connection),
             "esp32timer/%s/status/connection", device_id);
    snprintf(topic_status_telemetry, sizeof(topic_status_telemetry),
             "esp32timer/%s/status/telemetry", device_id);
    snprintf(topic_shadow_update, sizeof(topic_shadow_update),
             "$aws/things/%s/shadow/update", device_id);
    snprintf(topic_shadow_delta, sizeof(topic_shadow_delta),
             "$aws/things/%s/shadow/update/delta", device_id);
    
    ESP_LOGI(TAG, "Command topic: %s", topic_commands);
    ESP_LOGI(TAG, "Status connection topic: %s", topic_status_connection);
    ESP_LOGI(TAG, "Status telemetry topic: %s", topic_status_telemetry);
    ESP_LOGI(TAG, "Shadow update topic: %s", topic_shadow_update);
    ESP_LOGI(TAG, "Shadow delta topic: %s", topic_shadow_delta);
}

/**
 * @brief Load reachability state from NVS
 */
static void load_reachability_state(void)
{
    nvs_handle_t nvs_handle;
    esp_err_t ret = nvs_open(IOT_STATE_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (ret == ESP_OK) {
        uint8_t reachable = 1;
        ret = nvs_get_u8(nvs_handle, IOT_REACHABLE_KEY, &reachable);
        if (ret == ESP_OK) {
            iot_reachable = (reachable == 1);
            ESP_LOGI(TAG, "Loaded reachability state from NVS: %s",
                     iot_reachable ? "reachable" : "unreachable");
        }
        nvs_close(nvs_handle);
    }
}

/**
 * @brief Save reachability state to NVS
 */
static void save_reachability_state(void)
{
    nvs_handle_t nvs_handle;
    esp_err_t ret = nvs_open(IOT_STATE_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (ret == ESP_OK) {
        uint8_t reachable = iot_reachable ? 1 : 0;
        nvs_set_u8(nvs_handle, IOT_REACHABLE_KEY, reachable);
        nvs_commit(nvs_handle);
        nvs_close(nvs_handle);
        ESP_LOGI(TAG, "Saved reachability state to NVS: %s",
                 iot_reachable ? "reachable" : "unreachable");
    }
}

/**
 * @brief MQTT event handler
 */
static void mqtt_event_handler(void *handler_args, esp_event_base_t base,
                               int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;
    
    switch ((esp_mqtt_event_id_t)event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT connected to IoT Core");
            mqtt_connected = true;
            break;
            
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "MQTT disconnected from IoT Core");
            mqtt_connected = false;
            break;
            
        case MQTT_EVENT_ERROR:
            ESP_LOGE(TAG, "MQTT error occurred");
            if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
                ESP_LOGE(TAG, "TCP transport error");
            } else if (event->error_handle->error_type == MQTT_ERROR_TYPE_CONNECTION_REFUSED) {
                ESP_LOGE(TAG, "Connection refused");
            }
            mqtt_connected = false;
            break;
            
        case MQTT_EVENT_DATA:
            ESP_LOGI(TAG, "MQTT data received on topic: %.*s",
                     event->topic_len, event->topic);
            
            // Check if this is a shadow delta message
            if (strstr(event->topic, "/shadow/update/delta") != NULL) {
                ESP_LOGI(TAG, "Shadow delta received, length: %d", event->data_len);
                
                // Store delta payload for processing
                if (event->data_len < sizeof(delta_payload)) {
                    memcpy(delta_payload, event->data, event->data_len);
                    delta_payload[event->data_len] = '\0';
                    delta_payload_len = event->data_len;
                    delta_received = true;
                } else {
                    ESP_LOGE(TAG, "Delta payload too large: %d bytes", event->data_len);
                }
            }
            break;
            
        default:
            break;
    }
}

esp_err_t iot_manager_init(void)
{
    ESP_LOGI(TAG, "Initializing IoT Core manager");
    
    // Check if IoT Core is enabled
    if (!IOT_CORE_ENABLED) {
        ESP_LOGI(TAG, "IoT Core disabled in config.h");
        return ESP_FAIL;
    }
    
    // Initialize certificate store
    esp_err_t ret = cert_store_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize certificate store");
        return ESP_FAIL;
    }
    
    // Check if credentials are available
    if (!cert_store_has_credentials()) {
        ESP_LOGI(TAG, "IoT Core credentials not found, operating in BLE-only mode");
        iot_reachable = false;
        return ESP_FAIL;
    }
    
    // Generate device ID and topics
    generate_device_id();
    generate_mqtt_topics();
    
    // Load reachability state from NVS
    load_reachability_state();
    
    // Load certificates from NVS
    static char device_cert[2048];
    static char private_key[2048];
    static char root_ca[2048];
    
    ret = cert_store_get_device_cert(device_cert, sizeof(device_cert));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to load device certificate");
        return ESP_FAIL;
    }
    
    ret = cert_store_get_private_key(private_key, sizeof(private_key));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to load private key");
        return ESP_FAIL;
    }
    
    ret = cert_store_get_root_ca(root_ca, sizeof(root_ca));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to load root CA");
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "Certificates loaded successfully");
    
    // Configure MQTT client
    char mqtt_uri[256];
    snprintf(mqtt_uri, sizeof(mqtt_uri), "mqtts://%s:%d", IOT_ENDPOINT, IOT_MQTT_PORT);
    
    ESP_LOGI(TAG, "MQTT URI: %s", mqtt_uri);
    ESP_LOGI(TAG, "MQTT Client ID: %s", device_id);
    
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = mqtt_uri,
        .broker.verification.certificate = root_ca,
        .credentials = {
            .authentication = {
                .certificate = device_cert,
                .key = private_key,
            },
            .client_id = device_id,
        },
        .session.keepalive = IOT_KEEP_ALIVE_SEC,
        .network.timeout_ms = IOT_CONNECTION_TIMEOUT_SEC * 1000,
        .session.disable_clean_session = false,
    };
    
    mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    if (mqtt_client == NULL) {
        ESP_LOGE(TAG, "Failed to initialize MQTT client");
        return ESP_FAIL;
    }
    
    // Register event handler
    ret = esp_mqtt_client_register_event(mqtt_client, ESP_EVENT_ANY_ID,
                                         mqtt_event_handler, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register MQTT event handler");
        esp_mqtt_client_destroy(mqtt_client);
        mqtt_client = NULL;
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "IoT Core manager initialized successfully");
    return ESP_OK;
}

/**
 * @brief Serialize device state to status message JSON
 * 
 * Creates a JSON string containing device telemetry for status messages.
 * Format: {"version":"1.0","device_id":"...","timestamp":...,"battery":...,"temperature":...,"relay_state":"...","schedule_mode":"...","wifi_rssi":...,"uptime_seconds":...}
 * 
 * @param state Pointer to device state structure
 * @param json_out Buffer to store JSON string (caller must free with cJSON_free)
 * @return ESP_OK on success, ESP_FAIL on error
 */
static esp_err_t serialize_status_message(const device_state_t *state, char **json_out)
{
    if (state == NULL || json_out == NULL) {
        ESP_LOGE(TAG, "Invalid parameters for status message serialization");
        return ESP_FAIL;
    }
    
    // Create JSON document for status message
    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        ESP_LOGE(TAG, "Failed to create JSON root object for status message");
        return ESP_FAIL;
    }
    
    // Add version field
    cJSON_AddStringToObject(root, "version", "1.0");
    
    // Add device ID
    cJSON_AddStringToObject(root, "device_id", device_id);
    
    // Add timestamp
    cJSON_AddNumberToObject(root, "timestamp", state->timestamp);
    
    // Add battery percentage
    cJSON_AddNumberToObject(root, "battery", state->battery);
    
    // Add temperature
    cJSON_AddNumberToObject(root, "temperature", state->temperature);
    
    // Add relay state
    char relay_state_str[2] = {state->relay_state, '\0'};
    cJSON_AddStringToObject(root, "relay_state", relay_state_str);
    
    // Add schedule mode
    char schedule_mode_str[2] = {state->schedule_mode, '\0'};
    cJSON_AddStringToObject(root, "schedule_mode", schedule_mode_str);
    
    // Get WiFi RSSI
    wifi_ap_record_t ap_info;
    int8_t wifi_rssi = 0;
    if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
        wifi_rssi = ap_info.rssi;
    } else {
        ESP_LOGW(TAG, "Failed to get WiFi RSSI, using 0");
    }
    cJSON_AddNumberToObject(root, "wifi_rssi", wifi_rssi);
    
    // Calculate uptime in seconds (using FreeRTOS tick count)
    uint32_t uptime_seconds = (xTaskGetTickCount() * portTICK_PERIOD_MS) / 1000;
    cJSON_AddNumberToObject(root, "uptime_seconds", uptime_seconds);
    
    // Convert to JSON string
    *json_out = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    
    if (*json_out == NULL) {
        ESP_LOGE(TAG, "Failed to serialize status message JSON");
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "Status message serialized: %d bytes", strlen(*json_out));
    return ESP_OK;
}

esp_err_t iot_manager_publish_status_message(const device_state_t *state)
{
    if (mqtt_client == NULL || !mqtt_connected) {
        ESP_LOGE(TAG, "Cannot publish status message: MQTT not connected");
        return ESP_FAIL;
    }
    
    if (state == NULL) {
        ESP_LOGE(TAG, "Cannot publish status message: NULL state pointer");
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "Publishing status message to telemetry topic");
    
    // Serialize device state to status message JSON
    char *json_str = NULL;
    esp_err_t ret = serialize_status_message(state, &json_str);
    if (ret != ESP_OK || json_str == NULL) {
        ESP_LOGE(TAG, "Failed to serialize status message");
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "Status message payload: %s", json_str);
    
    // Publish to telemetry topic
    int msg_id = esp_mqtt_client_publish(mqtt_client, topic_status_telemetry,
                                         json_str, 0, 1, 0);
    
    // Cleanup
    cJSON_free(json_str);
    
    if (msg_id < 0) {
        ESP_LOGE(TAG, "Failed to publish status message");
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "Status message published successfully (msg_id: %d)", msg_id);
    return ESP_OK;
}

esp_err_t iot_manager_publish_state(const device_state_t *state)
{
    if (mqtt_client == NULL || !mqtt_connected) {
        ESP_LOGE(TAG, "Cannot publish state: MQTT not connected");
        return ESP_FAIL;
    }
    
    if (state == NULL) {
        ESP_LOGE(TAG, "Cannot publish state: NULL state pointer");
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "Publishing device state to shadow");
    
    // Create JSON document for shadow update
    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        ESP_LOGE(TAG, "Failed to create JSON root object");
        return ESP_FAIL;
    }
    
    // Create state object
    cJSON *state_obj = cJSON_CreateObject();
    cJSON_AddItemToObject(root, "state", state_obj);
    
    // Create reported object
    cJSON *reported = cJSON_CreateObject();
    cJSON_AddItemToObject(state_obj, "reported", reported);
    
    // Add basic telemetry
    cJSON_AddNumberToObject(reported, "battery", state->battery);
    cJSON_AddNumberToObject(reported, "temperature", state->temperature);
    
    char relay_state_str[2] = {state->relay_state, '\0'};
    cJSON_AddStringToObject(reported, "relay_state", relay_state_str);
    
    cJSON_AddNumberToObject(reported, "timestamp", state->timestamp);
    
    char schedule_mode_str[2] = {state->schedule_mode, '\0'};
    cJSON_AddStringToObject(reported, "schedule_mode", schedule_mode_str);
    
    // Add thresholds
    cJSON *thresholds = cJSON_CreateObject();
    cJSON_AddNumberToObject(thresholds, "high", state->thresholds.high);
    cJSON_AddNumberToObject(thresholds, "low", state->thresholds.low);
    cJSON_AddItemToObject(reported, "thresholds", thresholds);
    
    // Add schedule
    cJSON *schedule = cJSON_CreateObject();
    const char *day_names[] = {"monday", "tuesday", "wednesday", "thursday", 
                               "friday", "saturday", "sunday"};
    for (int i = 0; i < 7; i++) {
        cJSON_AddStringToObject(schedule, day_names[i], state->schedule[i]);
    }
    cJSON_AddItemToObject(reported, "schedule", schedule);
    
    // Add calibration data
    cJSON *calibration = cJSON_CreateObject();
    
    // Temperature calibration points
    cJSON *temp_points = cJSON_CreateArray();
    for (int i = 0; i < state->calibration.temp_count && i < 10; i++) {
        cJSON *point = cJSON_CreateArray();
        cJSON_AddItemToArray(point, cJSON_CreateNumber(state->calibration.temp_points[i][0]));
        cJSON_AddItemToArray(point, cJSON_CreateNumber(state->calibration.temp_points[i][1]));
        cJSON_AddItemToArray(temp_points, point);
    }
    cJSON_AddItemToObject(calibration, "temp_points", temp_points);
    
    // Battery calibration points
    cJSON *battery_points = cJSON_CreateArray();
    for (int i = 0; i < state->calibration.battery_count && i < 10; i++) {
        cJSON *point = cJSON_CreateArray();
        cJSON_AddItemToArray(point, cJSON_CreateNumber(state->calibration.battery_points[i][0]));
        cJSON_AddItemToArray(point, cJSON_CreateNumber(state->calibration.battery_points[i][1]));
        cJSON_AddItemToArray(battery_points, point);
    }
    cJSON_AddItemToObject(calibration, "battery_points", battery_points);
    
    cJSON_AddItemToObject(reported, "calibration", calibration);
    
    // Convert to JSON string
    char *json_str = cJSON_PrintUnformatted(root);
    if (json_str == NULL) {
        ESP_LOGE(TAG, "Failed to serialize JSON");
        cJSON_Delete(root);
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "Shadow update payload size: %d bytes", strlen(json_str));
    
    // Publish to shadow update topic
    int msg_id = esp_mqtt_client_publish(mqtt_client, topic_shadow_update,
                                         json_str, 0, 1, 0);
    
    // Cleanup
    cJSON_free(json_str);
    cJSON_Delete(root);
    
    if (msg_id < 0) {
        ESP_LOGE(TAG, "Failed to publish shadow update");
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "Shadow update published successfully (msg_id: %d)", msg_id);
    return ESP_OK;
}

esp_err_t iot_manager_process_delta(void)
{
    if (mqtt_client == NULL || !mqtt_connected) {
        ESP_LOGE(TAG, "Cannot process delta: MQTT not connected");
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "Processing shadow delta updates");
    
    // Check if we already received a delta (it may have arrived after shadow publish)
    if (delta_received) {
        ESP_LOGI(TAG, "Delta already received (length: %d), processing immediately", delta_payload_len);
    } else {
        // Wait a bit for delta to arrive (it may be in flight)
        ESP_LOGI(TAG, "Waiting for delta to arrive...");
        
        int timeout_ms = 3000;  // 3 second timeout
        int elapsed_ms = 0;
        int poll_interval_ms = 100;
        
        while (!delta_received && elapsed_ms < timeout_ms && mqtt_connected) {
            vTaskDelay(pdMS_TO_TICKS(poll_interval_ms));
            elapsed_ms += poll_interval_ms;
        }
        
        if (!mqtt_connected) {
            ESP_LOGE(TAG, "MQTT connection lost while waiting for delta");
            return ESP_FAIL;
        }
        
        if (!delta_received) {
            ESP_LOGI(TAG, "No delta updates pending after %d seconds", timeout_ms / 1000);
            return ESP_OK;
        }
    }
    
    ESP_LOGI(TAG, "Processing delta payload: %s", delta_payload);
    
    // Parse delta JSON
    cJSON *root = cJSON_Parse(delta_payload);
    if (root == NULL) {
        ESP_LOGE(TAG, "Failed to parse delta JSON");
        return ESP_FAIL;
    }
    
    cJSON *state = cJSON_GetObjectItem(root, "state");
    if (state == NULL) {
        ESP_LOGE(TAG, "Delta JSON missing 'state' field");
        cJSON_Delete(root);
        return ESP_FAIL;
    }
    
    // Track what was updated for confirmation
    bool updated = false;
    cJSON *confirmation_reported = cJSON_CreateObject();
    
    // Process override command
    cJSON *override = cJSON_GetObjectItem(state, "override");
    if (override != NULL && cJSON_IsObject(override)) {
        cJSON *active = cJSON_GetObjectItem(override, "active");
        cJSON *duration = cJSON_GetObjectItem(override, "duration_minutes");
        
        if (active != NULL && cJSON_IsBool(active) && 
            duration != NULL && cJSON_IsNumber(duration)) {
            
            bool override_active = cJSON_IsTrue(active);
            int duration_minutes = duration->valueint;
            
            ESP_LOGI(TAG, "Delta: override active=%d, duration=%d minutes", 
                     override_active, duration_minutes);
            
            if (override_active && duration_minutes > 0) {
                // Activate manual override
                gpio_set_relay_manual(1, duration_minutes);
                ESP_LOGI(TAG, "Manual override activated for %d minutes", duration_minutes);
            } else {
                // Clear manual override
                gpio_clear_manual_override();
                ESP_LOGI(TAG, "Manual override cleared");
            }
            
            // Add to confirmation
            cJSON *override_confirm = cJSON_CreateObject();
            cJSON_AddBoolToObject(override_confirm, "active", override_active);
            cJSON_AddNumberToObject(override_confirm, "duration_minutes", duration_minutes);
            cJSON_AddItemToObject(confirmation_reported, "override", override_confirm);
            updated = true;
        } else {
            ESP_LOGW(TAG, "Invalid override format in delta");
        }
    }
    
    // Process threshold update
    cJSON *thresholds = cJSON_GetObjectItem(state, "thresholds");
    if (thresholds != NULL && cJSON_IsObject(thresholds)) {
        cJSON *high = cJSON_GetObjectItem(thresholds, "high");
        cJSON *low = cJSON_GetObjectItem(thresholds, "low");
        
        if (high != NULL && cJSON_IsNumber(high) && 
            low != NULL && cJSON_IsNumber(low)) {
            
            float high_temp = (float)high->valuedouble;
            float low_temp = (float)low->valuedouble;
            
            ESP_LOGI(TAG, "Delta: thresholds high=%.1f, low=%.1f", high_temp, low_temp);
            
            // Update thresholds via scheduler
            esp_err_t ret = scheduler_set_temperatures((int16_t)high_temp, (int16_t)low_temp);
            if (ret == ESP_OK) {
                ESP_LOGI(TAG, "Temperature thresholds updated");
                
                // Add to confirmation
                cJSON *thresholds_confirm = cJSON_CreateObject();
                cJSON_AddNumberToObject(thresholds_confirm, "high", high_temp);
                cJSON_AddNumberToObject(thresholds_confirm, "low", low_temp);
                cJSON_AddItemToObject(confirmation_reported, "thresholds", thresholds_confirm);
                updated = true;
            } else {
                ESP_LOGE(TAG, "Failed to update temperature thresholds");
            }
        } else {
            ESP_LOGW(TAG, "Invalid thresholds format in delta");
        }
    }
    
    // Process schedule update
    cJSON *schedule = cJSON_GetObjectItem(state, "schedule");
    if (schedule != NULL && cJSON_IsObject(schedule)) {
        const char *day_names[] = {"monday", "tuesday", "wednesday", "thursday", 
                                   "friday", "saturday", "sunday"};
        cJSON *schedule_confirm = cJSON_CreateObject();
        bool schedule_updated = false;
        
        for (int i = 0; i < 7; i++) {
            cJSON *day_schedule = cJSON_GetObjectItem(schedule, day_names[i]);
            if (day_schedule != NULL && cJSON_IsString(day_schedule)) {
                const char *schedule_str = day_schedule->valuestring;
                
                ESP_LOGI(TAG, "Delta: schedule %s=%s", day_names[i], schedule_str);
                
                // Update schedule via scheduler
                esp_err_t ret = scheduler_set_day_schedule(i, schedule_str);
                if (ret == ESP_OK) {
                    cJSON_AddStringToObject(schedule_confirm, day_names[i], schedule_str);
                    schedule_updated = true;
                } else {
                    ESP_LOGE(TAG, "Failed to update schedule for %s", day_names[i]);
                }
            }
        }
        
        if (schedule_updated) {
            ESP_LOGI(TAG, "Schedule updated");
            cJSON_AddItemToObject(confirmation_reported, "schedule", schedule_confirm);
            updated = true;
        } else {
            cJSON_Delete(schedule_confirm);
        }
    }
    
    // Process calibration update
    cJSON *calibration = cJSON_GetObjectItem(state, "calibration");
    if (calibration != NULL && cJSON_IsObject(calibration)) {
        ESP_LOGI(TAG, "Delta: calibration update received");
        
        // Note: Calibration updates would require more complex handling
        // For now, just log that we received it
        ESP_LOGW(TAG, "Calibration updates not yet implemented");
    }
    
    // Cleanup delta JSON
    cJSON_Delete(root);
    
    // Publish confirmation if anything was updated
    if (updated) {
        ESP_LOGI(TAG, "Publishing delta confirmation to shadow");
        
        // Create confirmation JSON
        cJSON *confirm_root = cJSON_CreateObject();
        cJSON *confirm_state = cJSON_CreateObject();
        cJSON_AddItemToObject(confirm_root, "state", confirm_state);
        cJSON_AddItemToObject(confirm_state, "reported", confirmation_reported);
        
        char *confirm_json = cJSON_PrintUnformatted(confirm_root);
        if (confirm_json != NULL) {
            int msg_id = esp_mqtt_client_publish(mqtt_client, topic_shadow_update,
                                                 confirm_json, 0, 1, 0);
            if (msg_id >= 0) {
                ESP_LOGI(TAG, "Delta confirmation published (msg_id: %d)", msg_id);
            } else {
                ESP_LOGE(TAG, "Failed to publish delta confirmation");
            }
            cJSON_free(confirm_json);
        }
        cJSON_Delete(confirm_root);
    } else {
        cJSON_Delete(confirmation_reported);
    }
    
    // Clear delta flag for next sync session
    delta_received = false;
    delta_payload_len = 0;
    
    return ESP_OK;
}

esp_err_t iot_manager_sync_session(void)
{
    if (mqtt_client == NULL) {
        ESP_LOGE(TAG, "MQTT client not initialized");
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "Starting IoT Core sync session");
    sync_in_progress = true;
    mqtt_connected = false;
    
    // Clear delta state from any previous session
    delta_received = false;
    delta_payload_len = 0;
    
    // Start MQTT client (connect)
    esp_err_t ret = esp_mqtt_client_start(mqtt_client);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start MQTT client: %s", esp_err_to_name(ret));
        sync_in_progress = false;
        iot_manager_mark_unreachable();
        return ESP_FAIL;
    }
    
    // Wait for connection with timeout
    int timeout_ms = IOT_CONNECTION_TIMEOUT_SEC * 1000;
    int elapsed_ms = 0;
    int poll_interval_ms = 100;
    
    while (!mqtt_connected && elapsed_ms < timeout_ms) {
        vTaskDelay(pdMS_TO_TICKS(poll_interval_ms));
        elapsed_ms += poll_interval_ms;
    }
    
    if (!mqtt_connected) {
        ESP_LOGE(TAG, "MQTT connection timeout after %d seconds", IOT_CONNECTION_TIMEOUT_SEC);
        esp_mqtt_client_stop(mqtt_client);
        sync_in_progress = false;
        iot_manager_mark_unreachable();
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "MQTT connected successfully");
    
    // Subscribe to shadow delta topic FIRST (before publishing state)
    // This ensures we receive any pending deltas
    int msg_id = esp_mqtt_client_subscribe(mqtt_client, topic_shadow_delta, 1);
    if (msg_id < 0) {
        ESP_LOGE(TAG, "Failed to subscribe to shadow delta topic");
        esp_mqtt_client_stop(mqtt_client);
        sync_in_progress = false;
        iot_manager_mark_unreachable();
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "Subscribed to shadow delta topic: %s", topic_shadow_delta);
    
    // Give AWS IoT Core time to process the subscription
    vTaskDelay(pdMS_TO_TICKS(500));
    
    // Subscribe to command topic
    msg_id = esp_mqtt_client_subscribe(mqtt_client, topic_commands, 1);
    if (msg_id < 0) {
        ESP_LOGE(TAG, "Failed to subscribe to command topic");
        esp_mqtt_client_stop(mqtt_client);
        sync_in_progress = false;
        iot_manager_mark_unreachable();
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "Subscribed to command topic: %s", topic_commands);
    
    // Publish connection status
    const char *status_msg = "{\"status\":\"connected\",\"timestamp\":%lld}";
    char status_payload[128];
    snprintf(status_payload, sizeof(status_payload), status_msg, (long long)time(NULL));
    
    msg_id = esp_mqtt_client_publish(mqtt_client, topic_status_connection,
                                     status_payload, 0, 1, 0);
    if (msg_id < 0) {
        ESP_LOGE(TAG, "Failed to publish connection status");
        esp_mqtt_client_stop(mqtt_client);
        sync_in_progress = false;
        iot_manager_mark_unreachable();
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "Published connection status");
    
    // Gather current device state
    device_state_t device_state = {0};
    
    // Get battery and temperature
    device_state.battery = gpio_get_battery_percentage();
    device_state.temperature = (float)gpio_read_temperature();
    device_state.relay_state = gpio_get_relay() ? 'H' : 'L';
    device_state.timestamp = (int64_t)time(NULL);
    device_state.schedule_mode = scheduler_get_current_mode();
    
    // Get scheduler configuration
    scheduler_config_t sched_config;
    ret = scheduler_get_config(&sched_config);
    if (ret == ESP_OK) {
        device_state.thresholds.high = (float)sched_config.high_temp;
        device_state.thresholds.low = (float)sched_config.low_temp;
        
        // Copy schedule
        for (int i = 0; i < 7; i++) {
            strncpy(device_state.schedule[i], sched_config.schedule[i], 24);
            device_state.schedule[i][24] = '\0';
        }
    } else {
        ESP_LOGW(TAG, "Failed to get scheduler config, using defaults");
        device_state.thresholds.high = 22.0f;
        device_state.thresholds.low = 18.0f;
    }
    
    // Note: Calibration data would need to be retrieved from gpio_manager
    // For now, set counts to 0
    device_state.calibration.temp_count = 0;
    device_state.calibration.battery_count = 0;
    
    // Publish device state to shadow
    ret = iot_manager_publish_state(&device_state);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to publish device state to shadow");
        esp_mqtt_client_stop(mqtt_client);
        sync_in_progress = false;
        iot_manager_mark_unreachable();
        return ESP_FAIL;
    }
    
    // Publish status message to telemetry topic
    ret = iot_manager_publish_status_message(&device_state);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to publish status message");
        // Don't fail the entire sync for status message errors
        // Just log and continue
    }
    
    // Process shadow delta updates
    ret = iot_manager_process_delta();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to process shadow delta");
        // Don't fail the entire sync for delta processing errors
        // Just log and continue
    }
    
    // Wait a moment to allow messages to be sent
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    // Disconnect MQTT client
    ret = esp_mqtt_client_stop(mqtt_client);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to stop MQTT client cleanly: %s", esp_err_to_name(ret));
    }
    
    mqtt_connected = false;
    sync_in_progress = false;
    
    ESP_LOGI(TAG, "IoT Core sync session completed successfully");
    
    // Mark as reachable since sync succeeded
    if (!iot_reachable) {
        iot_reachable = true;
        save_reachability_state();
        ESP_LOGI(TAG, "IoT Core marked as reachable");
    }
    
    return ESP_OK;
}

bool iot_manager_is_reachable(void)
{
    return iot_reachable;
}

void iot_manager_mark_unreachable(void)
{
    if (iot_reachable) {
        ESP_LOGW(TAG, "Marking IoT Core as unreachable, switching to BLE mode");
        iot_reachable = false;
        save_reachability_state();
    }
}

void iot_manager_deinit(void)
{
    if (mqtt_client != NULL) {
        if (mqtt_connected) {
            esp_mqtt_client_stop(mqtt_client);
        }
        esp_mqtt_client_destroy(mqtt_client);
        mqtt_client = NULL;
    }
    
    mqtt_connected = false;
    sync_in_progress = false;
    
    ESP_LOGI(TAG, "IoT Core manager deinitialized");
}
