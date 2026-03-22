#include <stdio.h>
#include <time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_sleep.h"
#include "esp_pm.h"
#include "nvs_flash.h"
#include "config.h"
#include "wifi_manager.h"
#include "ble_server.h"
#include "led_status.h"
#include "scheduler.h"
#include "gpio_manager.h"
#include "cert_store.h"
#include "iot_manager.h"
#include "connection_manager.h"
#include "provision_certs.h"

static const char *TAG = "main";

// Global flag to track if time was successfully synced
bool time_synced = false;

// Provision IoT Core credentials once (only if not already provisioned)
static void provision_iot_credentials_once(void)
{
    if (cert_store_has_credentials()) {
        ESP_LOGI(TAG, "IoT Core certificates already provisioned");
        return;
    }
    
    ESP_LOGI(TAG, "Provisioning IoT Core certificates...");
    esp_err_t ret = cert_store_set_credentials(DEVICE_CERT, PRIVATE_KEY, ROOT_CA);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "✓ IoT Core credentials provisioned successfully!");
    } else {
        ESP_LOGE(TAG, "✗ Failed to provision IoT Core credentials: %s", esp_err_to_name(ret));
    }
}

// Track light sleep start time (not in RTC memory, resets on deep sleep)
static uint32_t light_sleep_start_time = 0;

// Relay hysteresis tracking
static time_t last_relay_switch_time = 0;

// Helper function to check scheduler and update relay state
static void check_scheduler_and_update_relay(void)
{
    time_t now;
    struct tm timeinfo;
    
    time(&now);
    localtime_r(&now, &timeinfo);
    
    // Skip scheduler if manual override is active
    if (!gpio_is_manual_override_active()) 
    {
        // Check scheduler and control relay based on temperature (only if time is synced and no manual override)
        if (time_synced && timeinfo.tm_year > (2024 - 1900)) 
        {
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
    
    // Configure Power Management early (before BLE initialization)
    esp_pm_config_t pm_config = {
        .max_freq_mhz = PM_MAX_CPU_FREQ_MHZ,
        .min_freq_mhz = PM_MIN_CPU_FREQ_MHZ,
        .light_sleep_enable = true
    };
    esp_err_t pm_ret = esp_pm_configure(&pm_config);
    if (pm_ret == ESP_OK) {
        ESP_LOGI(TAG, "Power Management configured: max=%dMHz, min=%dMHz, light_sleep=enabled",
                 PM_MAX_CPU_FREQ_MHZ, PM_MIN_CPU_FREQ_MHZ);
    } else {
        ESP_LOGW(TAG, "Power Management configuration failed: %s (continuing without PM)", 
                 esp_err_to_name(pm_ret));
    }
    
    // Initialize NVS (needed for connection_manager and other modules)
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
    
    // Initialize GPIO manager early (needed for ADC readings in IoT mode)
    ret = gpio_manager_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "GPIO manager init failed: %s", esp_err_to_name(ret));
        return;
    }
    ESP_LOGI(TAG, "GPIO manager initialized");
    
    // Initialize scheduler (loads configuration from NVS)
    ret = scheduler_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Scheduler init failed: %s", esp_err_to_name(ret));
        return;
    }
    ESP_LOGI(TAG, "Scheduler initialized");
    
    // Initialize connection manager (reads RTC state to determine mode)
    connection_manager_init();
    connection_mode_t mode = connection_manager_get_mode();
    
    ESP_LOGI(TAG, "=== Connection Mode: %s ===", 
             mode == CONNECTION_MODE_IOT ? "IoT Core (5-min cycle)" : "BLE Fallback (1-min cycle)");
    
    // Check wakeup reason
    esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
    
    if (wakeup_reason == ESP_SLEEP_WAKEUP_TIMER) {
        ESP_LOGI(TAG, "Woke up from deep sleep (timer)");
        
        // Print actual wake time
        time_t now;
        struct tm timeinfo;
        time(&now);
        localtime_r(&now, &timeinfo);
        
        if (mode == CONNECTION_MODE_IOT) {
            ESP_LOGI(TAG, "Actual wake time: %02d:%02d:%02d (IoT mode - 5-min cycle)", 
                     timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
        } else {
            ESP_LOGI(TAG, "Actual wake time: %02d:%02d:%02d (expected XX:XX:59 for BLE mode)", 
                     timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
        }
        
        // Check if we need to resync NTP (every 6 hours)
        bool need_ntp_sync = should_resync_ntp();
        
        if (need_ntp_sync || mode == CONNECTION_MODE_IOT || connection_manager_should_retry_iot()) {
            ESP_LOGI(TAG, "Connecting to WiFi...");
            
            // Initialize LED
            led_status_init();
            led_status_set(LED_STATUS_SYNCING);
            
            // Connect to WiFi
            wifi_init_sta();
            wifi_wait_connected();
            
            // Sync time with NTP if needed
            if (need_ntp_sync) {
                ESP_LOGI(TAG, "Resyncing time with NTP...");
                if (sync_time_with_ntp() == ESP_OK) {
                    ESP_LOGI(TAG, "Time resynchronization successful");
                    time_synced = true;
                    save_ntp_sync_time();
                    led_status_set(LED_STATUS_SYNCED);
                } else {
                    ESP_LOGE(TAG, "Time resynchronization failed");
                    time_synced = false;
                }
            } else {
                // Time is still valid from previous sync
                time_synced = true;
                ESP_LOGI(TAG, "Using previously synced time");
            }
            
            // Attempt IoT Core sync if in IoT mode or hourly retry
            if (mode == CONNECTION_MODE_IOT || connection_manager_should_retry_iot()) {
                ESP_LOGI(TAG, "Attempting IoT Core sync session...");
                
                // Initialize IoT manager if not already done
                esp_err_t iot_ret = iot_manager_init();
                if (iot_ret == ESP_OK) {
                    // Attempt sync session (WiFi already connected)
                    iot_ret = iot_manager_sync_session();
                    
                    if (iot_ret == ESP_OK) {
                        ESP_LOGI(TAG, "✓ IoT Core sync successful!");
                        connection_manager_update_mode(true);  // Mark as reachable
                        
                        // Update mode after successful sync
                        mode = connection_manager_get_mode();
                        
                        // Disconnect WiFi immediately after successful sync (within 1 second)
                        ESP_LOGI(TAG, "Disconnecting WiFi immediately (IoT mode power optimization)");
                        wifi_manager_disconnect_immediate();
                        
                        // In IoT mode, skip BLE and go straight to deep sleep
                        if (mode == CONNECTION_MODE_IOT) {
                            ESP_LOGI(TAG, "IoT mode: Skipping BLE, entering deep sleep");
                            
                            // Check scheduler before sleep
                            check_scheduler_and_update_relay();
                            
                            // Turn off LED
                            led_status_set(LED_STATUS_OFF);
                            
                            // Calculate sleep duration (5 minutes for IoT mode)
                            uint32_t sleep_duration_sec = connection_manager_get_sleep_duration_sec();
                            uint64_t deep_sleep_us = sleep_duration_sec * 1000000ULL;
                            
                            ESP_LOGI(TAG, "Entering deep sleep for %lu seconds (IoT mode)", sleep_duration_sec);
                            
                            // Configure deep sleep timer wakeup
                            esp_sleep_enable_timer_wakeup(deep_sleep_us);
                            
                            vTaskDelay(pdMS_TO_TICKS(100));  // Allow log to flush
                            
                            // Enter deep sleep (never returns - device will reset)
                            esp_deep_sleep_start();
                        }
                    } else {
                        ESP_LOGE(TAG, "✗ IoT Core sync failed: %s", esp_err_to_name(iot_ret));
                        
                        // Immediate WiFi disconnect on error to minimize power waste
                        ESP_LOGI(TAG, "Disconnecting WiFi immediately due to sync failure");
                        wifi_manager_disconnect_immediate();
                        
                        connection_manager_update_mode(false);  // Mark as unreachable, switch to BLE
                        mode = connection_manager_get_mode();
                        ESP_LOGI(TAG, "Switched to BLE mode due to IoT failure");
                    }
                } else {
                    ESP_LOGW(TAG, "IoT Core manager initialization failed: %s", esp_err_to_name(iot_ret));
                    
                    // Immediate WiFi disconnect on error to minimize power waste
                    ESP_LOGI(TAG, "Disconnecting WiFi immediately due to init failure");
                    wifi_manager_disconnect_immediate();
                    
                    connection_manager_update_mode(false);  // Mark as unreachable
                    mode = connection_manager_get_mode();
                }
            }
            
            // Disconnect WiFi if still connected (BLE mode after successful NTP sync)
            if (mode == CONNECTION_MODE_BLE && wifi_manager_is_connected()) {
                ESP_LOGI(TAG, "Disconnecting WiFi to save power (BLE mode)...");
                wifi_manager_disconnect_immediate();
            }
        } else {
            // Time is still valid from previous sync, no IoT sync needed
            time_synced = true;
            ESP_LOGI(TAG, "Using previously synced time, no WiFi connection needed");
        }
    } else {
        // First boot or reset
        ESP_LOGI(TAG, "First boot or reset");
        
        // Initialize LED (red - syncing)
        led_status_init();
        led_status_set(LED_STATUS_SYNCING);
        
        // Initialize and connect to WiFi
        wifi_init_sta();
        wifi_wait_connected();
        
        // Provision IoT Core credentials if not already done
        provision_iot_credentials_once();
        
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
            led_status_set(LED_STATUS_SYNCED);
        } else {
            ESP_LOGE(TAG, "Time synchronization failed");
            time_synced = false;
        }
        
        // Initialize IoT Core manager and attempt first sync
        ESP_LOGI(TAG, "Initializing IoT Core manager...");
        esp_err_t iot_ret = iot_manager_init();
        if (iot_ret == ESP_OK) {
            ESP_LOGI(TAG, "✓ IoT Core manager initialized successfully");
            
            // Test IoT Core sync session (WiFi already connected)
            ESP_LOGI(TAG, "Testing IoT Core sync session...");
            iot_ret = iot_manager_sync_session();
            if (iot_ret == ESP_OK) {
                ESP_LOGI(TAG, "✓ IoT Core sync successful!");
                connection_manager_update_mode(true);  // Mark as reachable
            } else {
                ESP_LOGE(TAG, "✗ IoT Core sync failed: %s", esp_err_to_name(iot_ret));
                connection_manager_update_mode(false);  // Mark as unreachable
            }
            
            // Update mode after first sync attempt
            mode = connection_manager_get_mode();
        } else {
            ESP_LOGW(TAG, "IoT Core manager initialization failed: %s (continuing in BLE-only mode)", 
                     esp_err_to_name(iot_ret));
            connection_manager_update_mode(false);  // Mark as unreachable
            mode = connection_manager_get_mode();
        }
        
        // Disconnect WiFi to save power
        ESP_LOGI(TAG, "Disconnecting WiFi to save power...");
        wifi_manager_disconnect_immediate();
        
        // Log current mode for debugging
        ESP_LOGI(TAG, "Current mode after first boot: %s (value: %d)", 
                 mode == CONNECTION_MODE_IOT ? "IoT" : "BLE", mode);
        
        // If IoT mode after successful first sync, enter deep sleep for 5 minutes
        if (mode == CONNECTION_MODE_IOT) {
            ESP_LOGI(TAG, "IoT mode: entering deep sleep after first boot sync");
            
            // Check scheduler before sleep
            check_scheduler_and_update_relay();
            
            // Turn off LED
            led_status_set(LED_STATUS_OFF);
            
            // Sleep for 5 minutes (IoT mode cycle)
            uint32_t sleep_duration_sec = connection_manager_get_sleep_duration_sec();
            uint64_t deep_sleep_us = sleep_duration_sec * 1000000ULL;
            
            ESP_LOGI(TAG, "Entering deep sleep for %lu seconds (IoT mode)", sleep_duration_sec);
            
            esp_sleep_enable_timer_wakeup(deep_sleep_us);
            vTaskDelay(pdMS_TO_TICKS(100));  // Allow log to flush
            esp_deep_sleep_start();
        }
    }
    
    // === BLE MODE BEHAVIOR ===
    // Only activate BLE if in BLE mode (not in IoT mode)
    if (mode == CONNECTION_MODE_BLE) {
        ESP_LOGI(TAG, "=== BLE Mode Active ===");
        
        // Initialize LED if not already done and set appropriate status
        if (wakeup_reason == ESP_SLEEP_WAKEUP_TIMER && !should_resync_ntp()) {
            led_status_init();
            
            // Check battery level first (highest priority for warning)
            uint8_t battery_percentage = gpio_get_battery_percentage();
            ESP_LOGI(TAG, "Battery level: %d%%", battery_percentage);
            
            if (battery_percentage < BATTERY_LOW_THRESHOLD_PERCENT) {
                led_status_set(LED_STATUS_LOW_BATTERY);
                ESP_LOGI(TAG, "LED: Yellow (low battery: %d%%)", battery_percentage);
            } else if (time_synced) {
                led_status_set(LED_STATUS_SYNCED);
                ESP_LOGI(TAG, "LED: Green (time synced)");
            } else {
                led_status_set(LED_STATUS_SYNCING);
                ESP_LOGI(TAG, "LED: Red (time not synced)");
            }
        }
        
        // Initialize BLE server
        if (ble_server_init() == ESP_OK) {
            ESP_LOGI(TAG, "BLE server initialized");
        } else {
            ESP_LOGE(TAG, "BLE server initialization failed");
        }
        
        // Wait for BLE connection for 6 seconds (XX:XX:59 to XX:XX:05)
        ESP_LOGI(TAG, "BLE advertising for %d seconds...", BLE_ADVERTISING_TIMEOUT_SEC);
        
        uint32_t start_time = xTaskGetTickCount();
        uint32_t timeout_ticks = pdMS_TO_TICKS(BLE_ADVERTISING_TIMEOUT_SEC * 1000);
        
        while ((xTaskGetTickCount() - start_time) < timeout_ticks) {
            if (ble_is_connected()) {
                ESP_LOGI(TAG, "BLE client connected, staying awake");
                
                // Check battery level
                uint8_t battery_percentage = gpio_get_battery_percentage();
                
                if (battery_percentage < BATTERY_LOW_THRESHOLD_PERCENT) {
                    led_status_set(LED_STATUS_LOW_BATTERY);
                    ESP_LOGI(TAG, "LED: Yellow (low battery: %d%%)", battery_percentage);
                } else {
                    led_status_set(LED_STATUS_BLE_CONNECTED);
                    ESP_LOGI(TAG, "LED: Blue (BLE connected)");
                }
                
                // Stay connected and update time + check scheduler
                while (ble_is_connected()) {
                    time_t now;
                    struct tm timeinfo;
                    
                    time(&now);
                    localtime_r(&now, &timeinfo);
                    
                    // Update standard time characteristic
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
        
        // Record start time for light sleep period
        light_sleep_start_time = xTaskGetTickCount() * portTICK_PERIOD_MS / 1000;
        
        // Calculate precise sleep timing based on current time
        time_t now;
        struct tm timeinfo;
        time(&now);
        localtime_r(&now, &timeinfo);
        
        int current_second = timeinfo.tm_sec;
        ESP_LOGI(TAG, "Current time: %02d:%02d:%02d", 
                 timeinfo.tm_hour, timeinfo.tm_min, current_second);
        
        // Target sleep second is 5 (XX:XX:05)
        int target_sleep_second = 5;
        
        // If we're past second 5, we woke up late - enter deep sleep immediately
        if (current_second > target_sleep_second) {
            ESP_LOGI(TAG, "Woke late (past XX:XX:05), entering deep sleep immediately");
            
            // Check scheduler before sleep
            check_scheduler_and_update_relay();
            
            // Get final timestamp
            time(&now);
            localtime_r(&now, &timeinfo);
            ESP_LOGI(TAG, "Actual time before deep sleep: %02d:%02d:%02d", 
                     timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
            
            // Deinitialize BLE before deep sleep
            ESP_LOGI(TAG, "Deinitializing BLE for deep sleep");
            ble_server_deinit();
            
            // Turn off LED
            led_status_set(LED_STATUS_OFF);
            
            // Calculate sleep duration to wake at next XX:XX:59
            int sleep_start_second = timeinfo.tm_sec;
            int target_wake_second = 59;
            int sleep_duration_sec;
            
            if (sleep_start_second < target_wake_second) {
                sleep_duration_sec = target_wake_second - sleep_start_second;
            } else {
                sleep_duration_sec = (60 - sleep_start_second) + target_wake_second;
            }
            
            uint64_t deep_sleep_us = sleep_duration_sec * 1000000ULL;
            
            ESP_LOGI(TAG, "Sleeping from XX:XX:%02d for %d seconds to wake at XX:XX:59", 
                     sleep_start_second, sleep_duration_sec);
            
            // Configure deep sleep timer wakeup
            esp_sleep_enable_timer_wakeup(deep_sleep_us);
            
            ESP_LOGI(TAG, "Entering deep sleep for %d seconds", sleep_duration_sec);
            vTaskDelay(pdMS_TO_TICKS(100));  // Allow log to flush
            
            // Enter deep sleep (never returns - device will reset)
            esp_deep_sleep_start();
        }
        
        // We're before XX:XX:05 - normal operation
        int seconds_until_sleep = target_sleep_second - current_second;
        ESP_LOGI(TAG, "Will enter deep sleep in %d seconds (at XX:XX:05)", seconds_until_sleep);
        
        // Main loop: stay in light sleep, then transition to deep sleep at precise time
        while (1) {
            vTaskDelay(pdMS_TO_TICKS(1000));  // Check every second
            
            // If BLE is connected, reset the light sleep timer
            if (ble_is_connected()) {
                light_sleep_start_time = xTaskGetTickCount() * portTICK_PERIOD_MS / 1000;
                
                // Update scheduler while connected
                check_scheduler_and_update_relay();
                
                // Recalculate sleep timing since we're staying awake
                time(&now);
                localtime_r(&now, &timeinfo);
                current_second = timeinfo.tm_sec;
                
                if (current_second <= target_sleep_second) {
                    seconds_until_sleep = target_sleep_second - current_second;
                } else {
                    seconds_until_sleep = (60 - current_second) + target_sleep_second;
                }
            } else {
                // Not connected - check if it's time for deep sleep
                uint32_t current_time = xTaskGetTickCount() * portTICK_PERIOD_MS / 1000;
                uint32_t elapsed = current_time - light_sleep_start_time;
                
                // Get current second to check if we've reached XX:XX:05
                time(&now);
                localtime_r(&now, &timeinfo);
                current_second = timeinfo.tm_sec;
                
                // Enter deep sleep at XX:XX:05
                if (current_second == target_sleep_second) {
                    // Get final timestamp before sleep
                    time(&now);
                    localtime_r(&now, &timeinfo);
                    
                    ESP_LOGI(TAG, "Reached XX:XX:05, entering deep sleep");
                    ESP_LOGI(TAG, "Actual time before deep sleep: %02d:%02d:%02d", 
                             timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
                    
                    // Check scheduler one last time before deep sleep
                    check_scheduler_and_update_relay();
                    
                    // Deinitialize BLE before deep sleep
                    ESP_LOGI(TAG, "Deinitializing BLE for deep sleep");
                    ble_server_deinit();
                    
                    // Turn off LED
                    led_status_set(LED_STATUS_OFF);
                    
                    // Calculate deep sleep duration to wake at XX:XX:59
                    time(&now);
                    localtime_r(&now, &timeinfo);
                    int sleep_start_second = timeinfo.tm_sec;
                    
                    int target_wake_second = 59;
                    int sleep_duration_sec;
                    
                    if (sleep_start_second <= target_wake_second) {
                        sleep_duration_sec = target_wake_second - sleep_start_second;
                    } else {
                        sleep_duration_sec = (60 - sleep_start_second) + target_wake_second;
                    }
                    
                    // Add 1 second buffer to ensure we wake at or just after XX:XX:59
                    sleep_duration_sec += 1;
                    
                    uint64_t deep_sleep_us = sleep_duration_sec * 1000000ULL;
                    
                    ESP_LOGI(TAG, "Sleeping from XX:XX:%02d for %d seconds to wake at XX:XX:59", 
                             sleep_start_second, sleep_duration_sec);
                    
                    // Configure deep sleep timer wakeup
                    esp_sleep_enable_timer_wakeup(deep_sleep_us);
                    
                    ESP_LOGI(TAG, "Entering deep sleep for %d seconds", sleep_duration_sec);
                    vTaskDelay(pdMS_TO_TICKS(100));  // Allow log to flush
                    
                    // Enter deep sleep (never returns - device will reset)
                    esp_deep_sleep_start();
                }
            }
        }
    }
    
    // Should never reach here - if we do, force BLE mode and enter deep sleep
    ESP_LOGE(TAG, "Unexpected: reached end of app_main, forcing BLE mode entry");
    ESP_LOGE(TAG, "Mode was: %s (value: %d)", 
             mode == CONNECTION_MODE_IOT ? "IoT" : "BLE", mode);
    
    // Force entry to deep sleep to prevent device from hanging
    ESP_LOGI(TAG, "Forcing deep sleep entry as safety fallback");
    
    // Check scheduler before sleep
    check_scheduler_and_update_relay();
    
    // Turn off LED
    led_status_set(LED_STATUS_OFF);
    
    // Calculate sleep duration based on mode
    uint32_t sleep_duration_sec = connection_manager_get_sleep_duration_sec();
    uint64_t deep_sleep_us = sleep_duration_sec * 1000000ULL;
    
    ESP_LOGI(TAG, "Entering deep sleep for %lu seconds (fallback)", sleep_duration_sec);
    
    // Configure deep sleep timer wakeup
    esp_sleep_enable_timer_wakeup(deep_sleep_us);
    
    vTaskDelay(pdMS_TO_TICKS(100));  // Allow log to flush
    
    // Enter deep sleep (never returns - device will reset)
    esp_deep_sleep_start();
}
