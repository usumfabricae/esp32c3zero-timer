#include "connection_manager.h"
#include "esp_log.h"
#include "esp_sleep.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "config.h"

static const char *TAG = "connection_manager";

// NVS namespace and keys for persistent storage
#define NVS_NAMESPACE "conn_mgr"
#define NVS_KEY_REACHABLE "iot_reach"
#define NVS_KEY_LAST_RETRY "last_retry"

// RTC memory structure for state persistence across deep sleep
typedef struct {
    uint8_t iot_reachable;    // 1=reachable, 0=unreachable
    uint32_t wake_count;      // Counts wake cycles for hourly retry
} rtc_connection_state_t;

// RTC memory attribute - persists across deep sleep but not power cycles
static RTC_DATA_ATTR rtc_connection_state_t rtc_state = {
    .iot_reachable = 1,  // Default to IoT mode on first boot
    .wake_count = 0
};

// Current connection mode (derived from RTC state)
static connection_mode_t current_mode = CONNECTION_MODE_IOT;

/**
 * @brief Load reachability state from NVS
 * 
 * Loads IoT Core reachability state from NVS for persistence across power cycles.
 * If NVS has no stored state, uses RTC memory value (which defaults to reachable).
 */
static void load_state_from_nvs(void)
{
    nvs_handle_t nvs_handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    
    if (ret == ESP_OK) {
        uint8_t stored_reachable = 1;  // Default to reachable
        ret = nvs_get_u8(nvs_handle, NVS_KEY_REACHABLE, &stored_reachable);
        
        if (ret == ESP_OK) {
            // NVS has stored state - use it to override RTC memory
            // This handles the case where device lost power (RTC reset but NVS persists)
            rtc_state.iot_reachable = stored_reachable;
            ESP_LOGI(TAG, "Loaded reachability from NVS: %s", 
                     stored_reachable ? "reachable" : "unreachable");
        } else {
            ESP_LOGI(TAG, "No stored reachability in NVS, using RTC value: %s",
                     rtc_state.iot_reachable ? "reachable" : "unreachable");
        }
        
        nvs_close(nvs_handle);
    } else {
        ESP_LOGI(TAG, "NVS not available, using RTC reachability: %s",
                 rtc_state.iot_reachable ? "reachable" : "unreachable");
    }
}

/**
 * @brief Save reachability state to NVS
 * 
 * Persists IoT Core reachability state to NVS for survival across power cycles.
 */
static void save_state_to_nvs(void)
{
    nvs_handle_t nvs_handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    
    if (ret == ESP_OK) {
        ret = nvs_set_u8(nvs_handle, NVS_KEY_REACHABLE, rtc_state.iot_reachable);
        if (ret == ESP_OK) {
            nvs_commit(nvs_handle);
            ESP_LOGI(TAG, "Saved reachability to NVS: %s",
                     rtc_state.iot_reachable ? "reachable" : "unreachable");
        } else {
            ESP_LOGE(TAG, "Failed to save reachability to NVS: %s", esp_err_to_name(ret));
        }
        nvs_close(nvs_handle);
    } else {
        ESP_LOGE(TAG, "Failed to open NVS for saving: %s", esp_err_to_name(ret));
    }
}

void connection_manager_init(void)
{
    ESP_LOGI(TAG, "Initializing connection manager");
    
    // Check wakeup reason to determine if this is first boot or wake from deep sleep
    esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
    
    if (wakeup_reason == ESP_SLEEP_WAKEUP_TIMER) {
        // Woke from deep sleep - RTC memory is valid
        ESP_LOGI(TAG, "Woke from deep sleep, RTC state valid");
        ESP_LOGI(TAG, "RTC reachability: %s, wake_count: %lu",
                 rtc_state.iot_reachable ? "reachable" : "unreachable",
                 rtc_state.wake_count);
        
        // Increment wake count for hourly retry tracking
        rtc_state.wake_count++;
    } else {
        // First boot or power cycle - RTC memory reset
        ESP_LOGI(TAG, "First boot or power cycle, loading state from NVS");
        
        // Load state from NVS (which persists across power cycles)
        load_state_from_nvs();
        
        // Reset wake count on first boot
        rtc_state.wake_count = 0;
    }
    
    // Determine current mode based on reachability
    current_mode = rtc_state.iot_reachable ? CONNECTION_MODE_IOT : CONNECTION_MODE_BLE;
    
    ESP_LOGI(TAG, "Connection mode: %s", 
             current_mode == CONNECTION_MODE_IOT ? "IoT" : "BLE");
}

connection_mode_t connection_manager_get_mode(void)
{
    return current_mode;
}

uint32_t connection_manager_get_sleep_duration_sec(void)
{
    if (current_mode == CONNECTION_MODE_IOT) {
        // IoT mode: sleep for 5 minutes between sync sessions
        return IOT_DEEP_SLEEP_DURATION_SEC;
    } else {
        // BLE mode: sleep for 54 seconds (wake at XX:XX:59 each minute)
        return DEEP_SLEEP_DURATION_SEC;
    }
}

void connection_manager_update_mode(bool iot_success)
{
    uint8_t new_reachable = iot_success ? 1 : 0;
    
    ESP_LOGI(TAG, "update_mode called: iot_success=%d, new_reachable=%d, current rtc_state.iot_reachable=%d",
             iot_success, new_reachable, rtc_state.iot_reachable);
    
    // Check if state changed
    if (new_reachable != rtc_state.iot_reachable) {
        ESP_LOGI(TAG, "IoT Core reachability changed: %s -> %s",
                 rtc_state.iot_reachable ? "reachable" : "unreachable",
                 new_reachable ? "reachable" : "unreachable");
        
        // Update RTC state
        rtc_state.iot_reachable = new_reachable;
        
        // Update current mode
        current_mode = new_reachable ? CONNECTION_MODE_IOT : CONNECTION_MODE_BLE;
        
        ESP_LOGI(TAG, "current_mode updated to: %s (value: %d)",
                 current_mode == CONNECTION_MODE_IOT ? "IoT" : "BLE", current_mode);
        
        // Persist to NVS for survival across power cycles
        save_state_to_nvs();
        
        // Reset wake count when switching modes
        rtc_state.wake_count = 0;
        
        ESP_LOGI(TAG, "Switched to %s mode", 
                 current_mode == CONNECTION_MODE_IOT ? "IoT" : "BLE");
    } else {
        ESP_LOGI(TAG, "IoT Core reachability unchanged: %s",
                 rtc_state.iot_reachable ? "reachable" : "unreachable");
    }
}

bool connection_manager_should_retry_iot(void)
{
    // Only retry in BLE mode
    if (current_mode != CONNECTION_MODE_BLE) {
        return false;
    }
    
    // Calculate how many wake cycles equal one hour
    // In BLE mode, device wakes every minute (60 seconds)
    // So 60 wake cycles = 1 hour
    uint32_t wakes_per_hour = IOT_RETRY_INTERVAL_SEC / 60;
    
    // Check if wake count is a multiple of wakes_per_hour
    bool should_retry = (rtc_state.wake_count % wakes_per_hour) == 0;
    
    if (should_retry) {
        ESP_LOGI(TAG, "Hourly IoT retry due (wake_count: %lu)", rtc_state.wake_count);
    }
    
    return should_retry;
}
