#include <stdio.h>
#include <time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_sleep.h"
#include "nvs_flash.h"
#include "config.h"
#include "wifi_manager.h"
#include "ble_server.h"
#include "led_status.h"
#include "scheduler.h"
#include "gpio_manager.h"

static const char *TAG = "main";

// Global flag to track if time was successfully synced
bool time_synced = false;

// Relay hysteresis tracking
static time_t last_relay_switch_time = 0;

// Helper function to check scheduler and update relay state
static void check_scheduler_and_update_relay(void)
{
    time_t now;
    struct tm timeinfo;
    
    time(&now);
    localtime_r(&now, &timeinfo);
    
    // Check scheduler and control relay based on temperature (only if time is synced and no manual override)
    if (time_synced && timeinfo.tm_year > (2024 - 1900)) {
        // Skip scheduler if manual override is active
        if (!gpio_is_manual_override_active()) {
            int16_t current_temp = gpio_read_temperature();
            bool should_be_on = scheduler_should_relay_be_on(current_temp);
            uint8_t current_relay_state = gpio_get_relay();
            
            // Check if state change is needed
            bool state_change_needed = (should_be_on && current_relay_state == 0) || 
                                       (!should_be_on && current_relay_state == 1);
            
            if (state_change_needed) {
                // Apply hysteresis: only switch if 5 minutes have passed since last switch
                time(&now);
                
                time_t time_since_last_switch = now - last_relay_switch_time;
                
                if (last_relay_switch_time == 0 || time_since_last_switch >= RELAY_HYSTERESIS_SEC) {
                    // Enough time has passed, allow the switch
                    if (should_be_on) {
                        ESP_LOGI(TAG, "Scheduler: Turning relay ON (temp-based)");
                        gpio_set_relay(1);
                    } else {
                        ESP_LOGI(TAG, "Scheduler: Turning relay OFF (temp-based)");
                        gpio_set_relay(0);
                    }
                    last_relay_switch_time = now;
                } else {
                    // Hysteresis active - prevent rapid switching
                    int remaining_sec = RELAY_HYSTERESIS_SEC - time_since_last_switch;
                    ESP_LOGD(TAG, "Relay switch blocked by hysteresis (%d sec remaining)", remaining_sec);
                }
            }
        }
    }
}

// NVS keys for time tracking
#define NVS_TIME_NAMESPACE "time_sync"
#define NVS_LAST_SYNC_KEY "last_sync"

static bool should_resync_ntp(void)
{
    nvs_handle_t nvs_handle;
    esp_err_t ret = nvs_open(NVS_TIME_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGI(TAG, "No previous NTP sync found, need to sync");
        return true;
    }
    
    int64_t last_sync_time = 0;
    ret = nvs_get_i64(nvs_handle, NVS_LAST_SYNC_KEY, &last_sync_time);
    nvs_close(nvs_handle);
    
    if (ret != ESP_OK) {
        ESP_LOGI(TAG, "No previous NTP sync timestamp, need to sync");
        return true;
    }
    
    time_t now;
    time(&now);
    int64_t elapsed = (int64_t)now - last_sync_time;
    
    ESP_LOGI(TAG, "Time since last NTP sync: %lld seconds", elapsed);
    
    if (elapsed >= NTP_RESYNC_INTERVAL_SEC) {
        ESP_LOGI(TAG, "NTP resync needed (>6 hours)");
        return true;
    }
    
    ESP_LOGI(TAG, "NTP sync still valid (%lld hours remaining)", 
             (NTP_RESYNC_INTERVAL_SEC - elapsed) / 3600);
    return false;
}

static void save_ntp_sync_time(void)
{
    nvs_handle_t nvs_handle;
    esp_err_t ret = nvs_open(NVS_TIME_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (ret == ESP_OK) {
        time_t now;
        time(&now);
        nvs_set_i64(nvs_handle, NVS_LAST_SYNC_KEY, (int64_t)now);
        nvs_commit(nvs_handle);
        nvs_close(nvs_handle);
        ESP_LOGI(TAG, "Saved NTP sync timestamp: %lld", (int64_t)now);
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "ESP32-C3 Project Started!");
    ESP_LOGI(TAG, "Device name: %s", DEVICE_NAME);
    
    // Check wakeup reason
    esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
    
    if (wakeup_reason == ESP_SLEEP_WAKEUP_TIMER) {
        ESP_LOGI(TAG, "Woke up from deep sleep (timer)");
        
        // Initialize NVS (needed for GPIO state restoration)
        esp_err_t ret = nvs_flash_init();
        if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
            ESP_ERROR_CHECK(nvs_flash_erase());
            ret = nvs_flash_init();
        }
        ESP_ERROR_CHECK(ret);
        
        // Set timezone (needs to be set on every boot)
        setenv("TZ", TIMEZONE_CONFIG, 1);
        tzset();
        ESP_LOGI(TAG, "Timezone configured: %s", TIMEZONE_CONFIG);
        
        // Check if we need to resync NTP (every 6 hours)
        if (should_resync_ntp()) {
            ESP_LOGI(TAG, "Resyncing time with NTP...");
            
            // Initialize LED
            led_status_init();
            led_status_set(LED_STATUS_SYNCING);
            
            // Connect to WiFi
            wifi_init_sta();
            wifi_wait_connected();
            
            // Sync time with NTP server
            if (sync_time_with_ntp() == ESP_OK) {
                ESP_LOGI(TAG, "Time resynchronization successful");
                time_synced = true;
                save_ntp_sync_time();
                led_status_set(LED_STATUS_SYNCED);
            } else {
                ESP_LOGE(TAG, "Time resynchronization failed");
                time_synced = false;
            }
            
            // Disconnect WiFi to save power
            ESP_LOGI(TAG, "Disconnecting WiFi to save power...");
            esp_wifi_disconnect();
            esp_wifi_stop();
            vTaskDelay(pdMS_TO_TICKS(500));
        } else {
            // Time is still valid from previous sync
            time_synced = true;
            ESP_LOGI(TAG, "Using previously synced time");
        }
    } else {
        ESP_LOGI(TAG, "First boot or reset");
        
        // Initialize LED (red - syncing)
        led_status_init();
        led_status_set(LED_STATUS_SYNCING);
        
        // Initialize and connect to WiFi (this initializes NVS)
        wifi_init_sta();
        wifi_wait_connected();
        
        // Set WiFi hostname
        esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
        if (netif) {
            esp_netif_set_hostname(netif, DEVICE_NAME);
            ESP_LOGI(TAG, "WiFi hostname set to: %s", DEVICE_NAME);
        }
        
        // Sync time with NTP server
        if (sync_time_with_ntp() == ESP_OK) {
            ESP_LOGI(TAG, "Time synchronization successful");
            time_synced = true;
            save_ntp_sync_time();
            // Set LED to green - time synced
            led_status_set(LED_STATUS_SYNCED);
        } else {
            ESP_LOGE(TAG, "Time synchronization failed");
            time_synced = false;
            // Keep LED red
        }
        
        // Disconnect WiFi to save power (time will continue from RTC)
        ESP_LOGI(TAG, "Disconnecting WiFi to save power...");
        esp_wifi_disconnect();
        esp_wifi_stop();
        
        // Small delay to ensure WiFi is fully stopped
        vTaskDelay(pdMS_TO_TICKS(500));
    }
    
    // Initialize LED if not already done and set appropriate status
    if (wakeup_reason == ESP_SLEEP_WAKEUP_TIMER && !should_resync_ntp()) {
        led_status_init();
        
        // Check battery level first (highest priority for warning)
        uint8_t battery_percentage = gpio_get_battery_percentage();
        ESP_LOGI(TAG, "Battery level: %d%%", battery_percentage);
        
        if (battery_percentage < BATTERY_LOW_THRESHOLD_PERCENT) {
            // Low battery - show yellow LED
            led_status_set(LED_STATUS_LOW_BATTERY);
            ESP_LOGI(TAG, "LED: Yellow (low battery: %d%%)", battery_percentage);
        } else if (time_synced) {
            // Show time sync status (green if synced, red if not)
            led_status_set(LED_STATUS_SYNCED);  // Green - time is synced
            ESP_LOGI(TAG, "LED: Green (time synced)");
        } else {
            led_status_set(LED_STATUS_SYNCING);  // Red - time not synced
            ESP_LOGI(TAG, "LED: Red (time not synced)");
        }
    }
    
    // Initialize BLE server
    if (ble_server_init() == ESP_OK) {
        ESP_LOGI(TAG, "BLE server initialized");
    } else {
        ESP_LOGE(TAG, "BLE server initialization failed");
    }
    
    // Wait for BLE connection for 10 seconds
    ESP_LOGI(TAG, "BLE advertising for %d seconds...", BLE_ADVERTISING_TIMEOUT_SEC);
    
    uint32_t start_time = xTaskGetTickCount();
    uint32_t timeout_ticks = pdMS_TO_TICKS(BLE_ADVERTISING_TIMEOUT_SEC * 1000);
    
    while ((xTaskGetTickCount() - start_time) < timeout_ticks) {
        if (ble_is_connected()) {
            ESP_LOGI(TAG, "BLE client connected, staying awake");
            
            // Check battery level
            uint8_t battery_percentage = gpio_get_battery_percentage();
            
            if (battery_percentage < BATTERY_LOW_THRESHOLD_PERCENT) {
                // Low battery takes priority - show yellow
                led_status_set(LED_STATUS_LOW_BATTERY);
                ESP_LOGI(TAG, "LED: Yellow (low battery: %d%%)", battery_percentage);
            } else {
                // BLE connected - show blue
                led_status_set(LED_STATUS_BLE_CONNECTED);
                ESP_LOGI(TAG, "LED: Blue (BLE connected)");
            }
            
            // Stay connected and update time + check scheduler
            while (ble_is_connected()) {
                time_t now;
                struct tm timeinfo;
                
                time(&now);
                localtime_r(&now, &timeinfo);
                
                // Update standard time characteristic (if handle is set)
                ble_update_standard_time();
                
                // Update standard temperature characteristic
                ble_update_standard_temperature();
                
                // Update battery level characteristic
                ble_update_battery_level();
                
                // Check scheduler and update relay
                check_scheduler_and_update_relay();
                
                vTaskDelay(pdMS_TO_TICKS(5000));  // Check every 5 seconds
            }
            
            ESP_LOGI(TAG, "BLE client disconnected");
            break;
        }
        
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    
    // No connection or disconnected, check scheduler before sleeping
    ESP_LOGI(TAG, "Checking scheduler before deep sleep...");
    check_scheduler_and_update_relay();
    
    ESP_LOGI(TAG, "No BLE connection, entering deep sleep for %d seconds", DEEP_SLEEP_DURATION_SEC);
    
    // Deinitialize BLE to save power
    ble_server_deinit();
    
    // Turn off LED
    led_status_set(LED_STATUS_OFF);
    
    // Configure timer wakeup
    esp_sleep_enable_timer_wakeup(DEEP_SLEEP_DURATION_SEC * 1000000ULL);
    
    // Enter deep sleep
    esp_deep_sleep_start();
}
