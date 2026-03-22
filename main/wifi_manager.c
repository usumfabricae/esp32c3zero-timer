#include "wifi_manager.h"
#include "config.h"
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_sntp.h"
#include "nvs_flash.h"
#include "nvs.h"

static const char *TAG = "wifi_manager";
static EventGroupHandle_t s_wifi_event_group;
static int s_retry_num = 0;
static bool s_wifi_connected = false;
static bool s_intentional_disconnect = false;

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

// NVS keys for WiFi configuration
#define NVS_WIFI_NAMESPACE "wifi_config"
#define NVS_WIFI_SSID_KEY "ssid"
#define NVS_WIFI_PASS_KEY "password"

// WiFi signal strength threshold for warning (dBm)
#define WIFI_WEAK_SIGNAL_THRESHOLD -80

static void event_handler(void* arg, esp_event_base_t event_base,
                         int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
        ESP_LOGI(TAG, "WiFi station started, connecting...");
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        s_wifi_connected = false;
        if (s_intentional_disconnect) {
            // Intentional disconnect - don't retry or log error
            ESP_LOGI(TAG, "WiFi disconnected (intentional)");
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        } else if (s_retry_num < WIFI_MAXIMUM_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "Retry to connect to the AP (attempt %d/%d)", s_retry_num, WIFI_MAXIMUM_RETRY);
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
            ESP_LOGE(TAG, "Failed to connect to WiFi after %d attempts", WIFI_MAXIMUM_RETRY);
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "Connected to WiFi! IP address: " IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        s_wifi_connected = true;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

// Load WiFi SSID from NVS, fall back to config.h default
static esp_err_t load_wifi_ssid(char *ssid, size_t max_len)
{
    nvs_handle_t nvs_handle;
    esp_err_t ret = nvs_open(NVS_WIFI_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGI(TAG, "No stored WiFi SSID found, using default from config.h: %s", WIFI_SSID);
        strncpy(ssid, WIFI_SSID, max_len - 1);
        ssid[max_len - 1] = '\0';
        return ESP_OK;
    }
    
    size_t len = max_len;
    ret = nvs_get_str(nvs_handle, NVS_WIFI_SSID_KEY, ssid, &len);
    nvs_close(nvs_handle);
    
    if (ret != ESP_OK) {
        ESP_LOGI(TAG, "Failed to read WiFi SSID from NVS, using default from config.h: %s", WIFI_SSID);
        strncpy(ssid, WIFI_SSID, max_len - 1);
        ssid[max_len - 1] = '\0';
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "Loaded WiFi SSID from NVS: %s", ssid);
    return ESP_OK;
}

// Load WiFi password from NVS, fall back to config.h default
static esp_err_t load_wifi_password(char *password, size_t max_len)
{
    nvs_handle_t nvs_handle;
    esp_err_t ret = nvs_open(NVS_WIFI_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGI(TAG, "No stored WiFi password found, using default from config.h");
        strncpy(password, WIFI_PASS, max_len - 1);
        password[max_len - 1] = '\0';
        return ESP_OK;
    }
    
    size_t len = max_len;
    ret = nvs_get_str(nvs_handle, NVS_WIFI_PASS_KEY, password, &len);
    nvs_close(nvs_handle);
    
    if (ret != ESP_OK) {
        ESP_LOGI(TAG, "Failed to read WiFi password from NVS, using default from config.h");
        strncpy(password, WIFI_PASS, max_len - 1);
        password[max_len - 1] = '\0';
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "Loaded WiFi password from NVS");
    return ESP_OK;
}

esp_err_t wifi_init_sta(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    // Load WiFi credentials from NVS (or use defaults from config.h)
    char ssid[33] = {0};
    char password[65] = {0};
    load_wifi_ssid(ssid, sizeof(ssid));
    load_wifi_password(password, sizeof(password));

    wifi_config_t wifi_config = {
        .sta = {
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };
    
    // Copy SSID and password to wifi_config
    strncpy((char *)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid) - 1);
    strncpy((char *)wifi_config.sta.password, password, sizeof(wifi_config.sta.password) - 1);

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "WiFi initialization complete. Connecting to SSID: %s", ssid);

    return ESP_OK;
}

void wifi_wait_connected(void)
{
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY);

    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "WiFi connection established successfully");
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGE(TAG, "Failed to connect to WiFi");
    }
}

static void time_sync_notification_cb(struct timeval *tv)
{
    ESP_LOGI(TAG, "Time synchronized with NTP server");
}

esp_err_t sync_time_with_ntp(void)
{
    ESP_LOGI(TAG, "Initializing SNTP client for NTP server: 0.it.pool.ntp.org");
    
    // Set timezone to Italy (CET/CEST with daylight saving time)
    setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1);
    tzset();
    ESP_LOGI(TAG, "Timezone set to CET/CEST (Italy)");
    
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "0.it.pool.ntp.org");
    esp_sntp_set_time_sync_notification_cb(time_sync_notification_cb);
    esp_sntp_init();

    // Wait for time to be set
    time_t now = 0;
    struct tm timeinfo = { 0 };
    int retry = 0;
    const int retry_count = 15;

    while (esp_sntp_get_sync_status() == SNTP_SYNC_STATUS_RESET && ++retry < retry_count) {
        ESP_LOGI(TAG, "Waiting for system time to be set... (%d/%d)", retry, retry_count);
        vTaskDelay(pdMS_TO_TICKS(2000));
    }

    if (retry >= retry_count) {
        ESP_LOGE(TAG, "Failed to synchronize time with NTP server");
        return ESP_FAIL;
    }

    time(&now);
    localtime_r(&now, &timeinfo);
    
    char strftime_buf[64];
    strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
    ESP_LOGI(TAG, "Current time synchronized: %s", strftime_buf);
    ESP_LOGI(TAG, "Unix timestamp: %ld", (long)now);

    return ESP_OK;
}

// Check if WiFi is currently connected
bool wifi_manager_is_connected(void)
{
    return s_wifi_connected;
}

// Connect WiFi with timeout (for IoT Core integration)
esp_err_t wifi_manager_connect_with_timeout(uint32_t timeout_sec)
{
    ESP_LOGI(TAG, "Connecting to WiFi with %lu second timeout", (unsigned long)timeout_sec);
    
    // Clear event bits before starting
    if (s_wifi_event_group) {
        xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT);
    }
    
    s_retry_num = 0;
    s_wifi_connected = false;
    
    // Start WiFi connection
    esp_err_t ret = esp_wifi_connect();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start WiFi connection: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Wait for connection with timeout
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE,
            pdFALSE,
            pdMS_TO_TICKS(timeout_sec * 1000));
    
    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "WiFi connected successfully within timeout");
        wifi_manager_check_signal_strength();
        return ESP_OK;
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGE(TAG, "WiFi connection failed after retries");
        return ESP_FAIL;
    } else {
        ESP_LOGE(TAG, "WiFi connection timeout after %lu seconds", (unsigned long)timeout_sec);
        return ESP_ERR_TIMEOUT;
    }
}

// Disconnect WiFi immediately (within 1 second)
esp_err_t wifi_manager_disconnect_immediate(void)
{
    ESP_LOGI(TAG, "Disconnecting WiFi immediately");
    
    if (!s_wifi_connected) {
        ESP_LOGI(TAG, "WiFi already disconnected");
        return ESP_OK;
    }
    
    // Set retry count to maximum to prevent event handler from reconnecting
    s_retry_num = WIFI_MAXIMUM_RETRY;
    s_intentional_disconnect = true;
    
    // Stop SNTP if running
    esp_sntp_stop();
    
    // Disconnect WiFi
    esp_err_t ret = esp_wifi_disconnect();
    if (ret != ESP_OK && ret != ESP_ERR_WIFI_NOT_STARTED) {
        ESP_LOGW(TAG, "WiFi disconnect returned: %s", esp_err_to_name(ret));
    }
    
    // Stop WiFi
    ret = esp_wifi_stop();
    if (ret != ESP_OK && ret != ESP_ERR_WIFI_NOT_STARTED) {
        ESP_LOGW(TAG, "WiFi stop returned: %s", esp_err_to_name(ret));
    }
    
    s_wifi_connected = false;
    
    // Give a brief moment for cleanup (100ms should be well under 1 second)
    vTaskDelay(pdMS_TO_TICKS(100));
    
    ESP_LOGI(TAG, "WiFi disconnected successfully");
    return ESP_OK;
}

// Check WiFi signal strength and log warning if weak
void wifi_manager_check_signal_strength(void)
{
    if (!s_wifi_connected) {
        ESP_LOGW(TAG, "Cannot check signal strength - WiFi not connected");
        return;
    }
    
    wifi_ap_record_t ap_info;
    esp_err_t ret = esp_wifi_sta_get_ap_info(&ap_info);
    
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to get AP info: %s", esp_err_to_name(ret));
        return;
    }
    
    int8_t rssi = ap_info.rssi;
    ESP_LOGI(TAG, "WiFi signal strength: %d dBm", rssi);
    
    if (rssi < WIFI_WEAK_SIGNAL_THRESHOLD) {
        ESP_LOGW(TAG, "WARNING: Weak WiFi signal detected (%d dBm < %d dBm threshold). Connection may be unstable.", 
                 rssi, WIFI_WEAK_SIGNAL_THRESHOLD);
    } else {
        ESP_LOGI(TAG, "WiFi signal strength is good");
    }
}
