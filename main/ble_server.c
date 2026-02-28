#include "ble_server.h"
#include "config.h"
#include "gpio_manager.h"
#include "scheduler.h"
#include <string.h>
#include "esp_log.h"
#include "esp_bt.h"
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_bt_main.h"
#include "esp_gatt_common_api.h"
#include "led_status.h"
#include "nvs_flash.h"
#include "nvs.h"
#include <time.h>

#define GATTS_TAG "ble_server"

// Custom Service
#define GATTS_SERVICE_UUID   0x00FF
#define GATTS_RELAY_CHAR_UUID 0xFF02
#define GATTS_PASSKEY_CHAR_UUID 0xFF04
#define GATTS_SCHEDULE_CHAR_UUID 0xFF05
#define GATTS_TEMP_CHAR_UUID 0xFF06
#define GATTS_TEMP_CAL_CHAR_UUID 0xFF07  // Temperature calibration
#define GATTS_BATT_CAL_CHAR_UUID 0xFF0B  // Battery calibration
#define GATTS_WIFI_SSID_CHAR_UUID 0xFF08  // WiFi SSID (read/write)
#define GATTS_WIFI_PASS_CHAR_UUID 0xFF09  // WiFi Password (write only)
#define GATTS_BLE_PASSKEY_CHAR_UUID 0xFF0A  // BLE Passkey (write only)
#define GATTS_NUM_HANDLE     37  // Increased for battery calibration characteristic

// Battery Service (Standard Bluetooth SIG)
#define BATTERY_SERVICE_UUID 0x180F
#define BATTERY_LEVEL_CHAR_UUID 0x2A19  // Battery Level (percentage 0-100)
#define BATTERY_NUM_HANDLE 4

// Standard characteristic UUIDs (added to custom service for compatibility)
#define STD_TEMPERATURE_CHAR_UUID  0x2A6E  // Environmental Sensing Temperature
#define STD_CURRENT_TIME_CHAR_UUID 0x2A2B  // Current Time
#define STD_MANUFACTURER_NAME_UUID 0x2A29  // Manufacturer Name
#define STD_MODEL_NUMBER_UUID      0x2A24  // Model Number
#define STD_FIRMWARE_REV_UUID      0x2A26  // Firmware Revision

// Device information strings
#define MANUFACTURER_NAME "ESP32"
#define MODEL_NUMBER "ESP32C3-Timer-v1"
#define FIRMWARE_REVISION "1.0.0"

// NVS keys for passkey storage
#define NVS_BLE_NAMESPACE "ble_security"
#define NVS_PASSKEY_KEY "passkey"

// NVS keys for WiFi configuration
#define NVS_WIFI_NAMESPACE "wifi_config"
#define NVS_WIFI_SSID_KEY "ssid"
#define NVS_WIFI_PASS_KEY "password"

#define PROFILE_APP_ID 0
#define GATTS_DEMO_CHAR_VAL_LEN_MAX 512  // Maximum attribute length for long reads
#define GATTS_CHUNK_SIZE 22  // Safe chunk size for MTU=23 (23 - 1 byte ATT opcode)

// External time sync status from main
extern bool time_synced;

// Custom service handles
static uint16_t relay_char_handle = 0;
static uint16_t passkey_char_handle = 0;
static uint16_t schedule_char_handle = 0;
static uint16_t temp_char_handle = 0;
static uint16_t temp_cal_char_handle = 0;
static uint16_t batt_cal_char_handle = 0;
static uint16_t wifi_ssid_char_handle = 0;
static uint16_t wifi_pass_char_handle = 0;
static uint16_t ble_passkey_char_handle = 0;
static uint16_t service_handle = 0;

// Battery service handles
static uint16_t battery_service_handle = 0;
static uint16_t battery_level_char_handle = 0;

// Schedule read buffer (for chunked reading)
static char schedule_read_buffer[200];  // 7 days * 24 chars + 6 CRs + null
static bool schedule_buffer_valid = false;
static uint16_t schedule_read_offset = 0;  // Current read offset for chunked reads

// Standard characteristic handles (to be added later)
static uint16_t std_temp_char_handle = 0;
static uint16_t std_time_char_handle = 0;
static uint16_t std_manufacturer_char_handle = 0;
static uint16_t std_model_char_handle = 0;
static uint16_t std_firmware_char_handle = 0;

static bool ble_connected = false;
static bool ble_authenticated = false;
static uint32_t current_passkey = DEFAULT_PASSKEY;

// Raw advertising data - will be updated with actual device name
static uint8_t raw_adv_data[31];  // Max BLE advertising data size

// Helper functions for standard services
static void format_current_time(uint8_t *buffer, size_t *len)
{
    time_t now;
    struct tm timeinfo;
    
    time(&now);
    localtime_r(&now, &timeinfo);
    
    // Current Time format (10 bytes):
    // Year (2 bytes), Month, Day, Hours, Minutes, Seconds, Day of Week, Fractions256, Adjust Reason
    uint16_t year = timeinfo.tm_year + 1900;
    buffer[0] = year & 0xFF;
    buffer[1] = (year >> 8) & 0xFF;
    buffer[2] = timeinfo.tm_mon + 1;  // Month (1-12)
    buffer[3] = timeinfo.tm_mday;     // Day (1-31)
    buffer[4] = timeinfo.tm_hour;     // Hours (0-23)
    buffer[5] = timeinfo.tm_min;      // Minutes (0-59)
    buffer[6] = timeinfo.tm_sec;      // Seconds (0-59)
    buffer[7] = (timeinfo.tm_wday == 0) ? 7 : timeinfo.tm_wday;  // Day of week (1=Monday, 7=Sunday)
    buffer[8] = 0;  // Fractions256 (1/256th of a second)
    buffer[9] = 0;  // Adjust reason (0 = No adjustment)
    
    *len = 10;
}

static void format_env_temperature(uint8_t *buffer, size_t *len)
{
    // Environmental Sensing Service Temperature format:
    // 2 bytes, signed 16-bit, resolution 0.01°C
    int16_t temp_celsius = gpio_read_temperature();
    int16_t temp_value = temp_celsius * 100;  // Convert to 0.01°C units
    
    buffer[0] = temp_value & 0xFF;
    buffer[1] = (temp_value >> 8) & 0xFF;
    
    *len = 2;
}

static uint8_t calculate_battery_percentage(uint16_t voltage_mv)
{
    // Use the gpio_manager function for consistency
    return gpio_get_battery_percentage();
}

static void build_schedule_read_buffer(void)
{
    scheduler_config_t config;
    scheduler_get_config(&config);
    
    // Build schedule string: 7 lines of 24 characters separated by \r\n
    int offset = 0;
    for (int day = 0; day < DAYS_PER_WEEK; day++) {
        // Copy 24-hour schedule for this day
        memcpy(&schedule_read_buffer[offset], config.schedule[day], HOURS_PER_DAY);
        offset += HOURS_PER_DAY;
        
        // Add carriage return after each day (except last)
        if (day < DAYS_PER_WEEK - 1) {
            schedule_read_buffer[offset++] = '\r';
            schedule_read_buffer[offset++] = '\n';
        }
    }
    schedule_read_buffer[offset] = '\0';
    schedule_buffer_valid = true;
    
    ESP_LOGI(GATTS_TAG, "Built schedule read buffer: %d bytes", offset);
}

// Security functions
static esp_err_t load_passkey_from_nvs(void)
{
    nvs_handle_t nvs_handle;
    esp_err_t ret = nvs_open(NVS_BLE_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGI(GATTS_TAG, "No stored passkey found, using default: %lu", (unsigned long)DEFAULT_PASSKEY);
        current_passkey = DEFAULT_PASSKEY;
        return ESP_OK;
    }
    
    ret = nvs_get_u32(nvs_handle, NVS_PASSKEY_KEY, &current_passkey);
    nvs_close(nvs_handle);
    
    if (ret != ESP_OK) {
        ESP_LOGI(GATTS_TAG, "Failed to read passkey, using default: %lu", (unsigned long)DEFAULT_PASSKEY);
        current_passkey = DEFAULT_PASSKEY;
        return ESP_OK;
    }
    
    ESP_LOGI(GATTS_TAG, "Loaded passkey from NVS: %lu", (unsigned long)current_passkey);
    return ESP_OK;
}

static esp_err_t save_passkey_to_nvs(uint32_t passkey)
{
    nvs_handle_t nvs_handle;
    esp_err_t ret = nvs_open(NVS_BLE_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(GATTS_TAG, "Failed to open NVS for passkey storage");
        return ret;
    }
    
    ret = nvs_set_u32(nvs_handle, NVS_PASSKEY_KEY, passkey);
    if (ret == ESP_OK) {
        ret = nvs_commit(nvs_handle);
        if (ret == ESP_OK) {
            ESP_LOGI(GATTS_TAG, "Passkey saved to NVS: %lu", (unsigned long)passkey);
        }
    }
    
    nvs_close(nvs_handle);
    return ret;
}

// WiFi configuration functions
static esp_err_t load_wifi_ssid_from_nvs(char *ssid, size_t max_len)
{
    nvs_handle_t nvs_handle;
    esp_err_t ret = nvs_open(NVS_WIFI_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGI(GATTS_TAG, "No stored WiFi SSID found, using default from config.h");
        strncpy(ssid, WIFI_SSID, max_len - 1);
        ssid[max_len - 1] = '\0';
        return ESP_OK;
    }
    
    size_t len = max_len;
    ret = nvs_get_str(nvs_handle, NVS_WIFI_SSID_KEY, ssid, &len);
    nvs_close(nvs_handle);
    
    if (ret != ESP_OK) {
        ESP_LOGI(GATTS_TAG, "Failed to read WiFi SSID, using default from config.h");
        strncpy(ssid, WIFI_SSID, max_len - 1);
        ssid[max_len - 1] = '\0';
        return ESP_OK;
    }
    
    ESP_LOGI(GATTS_TAG, "Loaded WiFi SSID from NVS: %s", ssid);
    return ESP_OK;
}

static esp_err_t save_wifi_ssid_to_nvs(const char *ssid)
{
    nvs_handle_t nvs_handle;
    esp_err_t ret = nvs_open(NVS_WIFI_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(GATTS_TAG, "Failed to open NVS for WiFi SSID storage");
        return ret;
    }
    
    ret = nvs_set_str(nvs_handle, NVS_WIFI_SSID_KEY, ssid);
    if (ret == ESP_OK) {
        ret = nvs_commit(nvs_handle);
        if (ret == ESP_OK) {
            ESP_LOGI(GATTS_TAG, "WiFi SSID saved to NVS: %s", ssid);
        }
    }
    
    nvs_close(nvs_handle);
    return ret;
}

static esp_err_t save_wifi_password_to_nvs(const char *password)
{
    nvs_handle_t nvs_handle;
    esp_err_t ret = nvs_open(NVS_WIFI_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(GATTS_TAG, "Failed to open NVS for WiFi password storage");
        return ret;
    }
    
    ret = nvs_set_str(nvs_handle, NVS_WIFI_PASS_KEY, password);
    if (ret == ESP_OK) {
        ret = nvs_commit(nvs_handle);
        if (ret == ESP_OK) {
            ESP_LOGI(GATTS_TAG, "WiFi password saved to NVS: %s", password);
        }
    }
    
    nvs_close(nvs_handle);
    return ret;
}

static esp_ble_adv_params_t adv_params = {
    .adv_int_min        = 0x20,
    .adv_int_max        = 0x40,
    .adv_type           = ADV_TYPE_IND,
    .own_addr_type      = BLE_ADDR_TYPE_PUBLIC,
    .channel_map        = ADV_CHNL_ALL,
    .adv_filter_policy  = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};

static void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
    switch (event) {
    case ESP_GAP_BLE_ADV_DATA_RAW_SET_COMPLETE_EVT:
        ESP_LOGI(GATTS_TAG, "Raw advertising data set complete, starting advertising");
        esp_ble_gap_start_advertising(&adv_params);
        break;
    case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
        if (param->adv_start_cmpl.status != ESP_BT_STATUS_SUCCESS) {
            ESP_LOGE(GATTS_TAG, "Advertising start failed, status: %d", param->adv_start_cmpl.status);
        } else {
            ESP_LOGI(GATTS_TAG, "Advertising started successfully");
        }
        break;
    case ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT:
        ESP_LOGI(GATTS_TAG, "Advertising stopped");
        break;
    case ESP_GAP_BLE_PASSKEY_REQ_EVT:
        ESP_LOGI(GATTS_TAG, "========================================");
        ESP_LOGI(GATTS_TAG, "PASSKEY REQUEST EVENT");
        ESP_LOGI(GATTS_TAG, "Phone is asking for passkey");
        ESP_LOGI(GATTS_TAG, "Replying with: %06lu", (unsigned long)current_passkey);
        ESP_LOGI(GATTS_TAG, "========================================");
        esp_ble_passkey_reply(param->ble_security.ble_req.bd_addr, true, current_passkey);
        break;
    case ESP_GAP_BLE_NC_REQ_EVT:
        ESP_LOGI(GATTS_TAG, "========================================");
        ESP_LOGI(GATTS_TAG, "NUMERIC COMPARISON REQUEST");
        ESP_LOGI(GATTS_TAG, "Passkey to compare: %06lu", (unsigned long)param->ble_security.key_notif.passkey);
        ESP_LOGI(GATTS_TAG, "Our passkey: %06lu", (unsigned long)current_passkey);
        ESP_LOGI(GATTS_TAG, "Match: %s", (param->ble_security.key_notif.passkey == current_passkey) ? "YES" : "NO");
        ESP_LOGI(GATTS_TAG, "Auto-confirming..."); 
        ESP_LOGI(GATTS_TAG, "========================================");
        esp_ble_confirm_reply(param->ble_security.ble_req.bd_addr, true);
        break;
    case ESP_GAP_BLE_PASSKEY_NOTIF_EVT:
        ESP_LOGI(GATTS_TAG, "========================================");
        ESP_LOGI(GATTS_TAG, "PASSKEY NOTIFICATION (Display on ESP32)");
        ESP_LOGI(GATTS_TAG, "BLE PAIRING PASSKEY: %06lu", (unsigned long)param->ble_security.key_notif.passkey);
        ESP_LOGI(GATTS_TAG, "Enter this passkey on your phone/laptop");
        ESP_LOGI(GATTS_TAG, "Configured passkey: %06lu", (unsigned long)current_passkey);
        ESP_LOGI(GATTS_TAG, "========================================");
        break;
    case ESP_GAP_BLE_KEY_EVT:
        ESP_LOGI(GATTS_TAG, "Key event - key_type: %d", param->ble_security.ble_key.key_type);
        break;
    case ESP_GAP_BLE_SEC_REQ_EVT:
        ESP_LOGI(GATTS_TAG, "Security request event from peer");
        esp_ble_gap_security_rsp(param->ble_security.ble_req.bd_addr, true);
        break;
    case ESP_GAP_BLE_AUTH_CMPL_EVT:
        if (param->ble_security.auth_cmpl.success) {
            ESP_LOGI(GATTS_TAG, "========================================");
            ESP_LOGI(GATTS_TAG, "Authentication SUCCESS! Device paired");
            ESP_LOGI(GATTS_TAG, "========================================");
            ble_authenticated = true;
            led_status_set(LED_STATUS_BLE_CONNECTED);
        } else {
            ESP_LOGE(GATTS_TAG, "========================================");
            ESP_LOGE(GATTS_TAG, "Authentication FAILED, reason: 0x%x", param->ble_security.auth_cmpl.fail_reason);
            ESP_LOGE(GATTS_TAG, "========================================");
            ble_authenticated = false;
        }
        break;
    case ESP_GAP_BLE_REMOVE_BOND_DEV_COMPLETE_EVT:
        ESP_LOGI(GATTS_TAG, "Bond device removed");
        break;
    default:
        break;
    }
}

static esp_gatt_if_t gatts_if_global = ESP_GATT_IF_NONE;

static void gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param)
{
    switch (event) {
    case ESP_GATTS_REG_EVT:
        ESP_LOGI(GATTS_TAG, "GATT server registered, status %d, app_id %d", param->reg.status, param->reg.app_id);
        gatts_if_global = gatts_if;
        
        // Configure raw advertising data - this will trigger ESP_GAP_BLE_ADV_DATA_RAW_SET_COMPLETE_EVT
        ESP_LOGI(GATTS_TAG, "Configuring raw advertising data");
        uint8_t adv_data_len = 0;
        for (int i = 0; i < sizeof(raw_adv_data); i++) {
            if (raw_adv_data[i] == 0 && i > 10) break;  // Find actual length
            adv_data_len = i + 1;
        }
        esp_err_t ret = esp_ble_gap_config_adv_data_raw(raw_adv_data, adv_data_len);
        if (ret) {
            ESP_LOGE(GATTS_TAG, "Config raw adv data failed, error code = %x", ret);
        }
        
        // Create GATT service
        esp_ble_gatts_create_service(gatts_if, &(esp_gatt_srvc_id_t){
            .is_primary = true,
            .id.inst_id = 0x00,
            .id.uuid.len = ESP_UUID_LEN_16,
            .id.uuid.uuid.uuid16 = GATTS_SERVICE_UUID,
        }, GATTS_NUM_HANDLE);
        break;
        
    case ESP_GATTS_CREATE_EVT:
        ESP_LOGI(GATTS_TAG, "Service created, status %d, service_handle %d", param->create.status, param->create.service_handle);
        
        if (param->create.service_id.id.uuid.uuid.uuid16 == GATTS_SERVICE_UUID) {
            // Custom service created
            service_handle = param->create.service_handle;
            esp_ble_gatts_start_service(service_handle);
            
            // Add relay control characteristic first
            esp_ble_gatts_add_char(service_handle, &(esp_bt_uuid_t){
                .len = ESP_UUID_LEN_16,
                .uuid.uuid16 = GATTS_RELAY_CHAR_UUID,
            }, ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
            ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_WRITE,
            &(esp_attr_value_t){
                .attr_max_len = 1,
                .attr_len = 0,
                .attr_value = NULL,
            }, NULL);
        } else if (param->create.service_id.id.uuid.uuid.uuid16 == BATTERY_SERVICE_UUID) {
            // Battery service created
            battery_service_handle = param->create.service_handle;
            esp_ble_gatts_start_service(battery_service_handle);
            ESP_LOGI(GATTS_TAG, "Battery Service started");
            
            // Add Battery Level characteristic
            // Note: NOTIFY property removed since notifications are not implemented
            esp_ble_gatts_add_char(battery_service_handle, &(esp_bt_uuid_t){
                .len = ESP_UUID_LEN_16,
                .uuid.uuid16 = BATTERY_LEVEL_CHAR_UUID,
            }, ESP_GATT_PERM_READ,
            ESP_GATT_CHAR_PROP_BIT_READ,
            &(esp_attr_value_t){
                .attr_max_len = 1,  // Battery level is 1 byte (0-100%)
                .attr_len = 0,
                .attr_value = NULL,
            }, NULL);
        }
        break;
        
    case ESP_GATTS_ADD_CHAR_EVT:
        ESP_LOGI(GATTS_TAG, "Characteristic added, status %d, attr_handle %d, uuid 0x%04x", 
                 param->add_char.status, param->add_char.attr_handle, param->add_char.char_uuid.uuid.uuid16);
        
        if (param->add_char.char_uuid.uuid.uuid16 == GATTS_RELAY_CHAR_UUID) {
            relay_char_handle = param->add_char.attr_handle;
            ESP_LOGI(GATTS_TAG, "Relay control characteristic added");
            
            // Add passkey change characteristic (write-only, for future use)
            esp_ble_gatts_add_char(service_handle, &(esp_bt_uuid_t){
                .len = ESP_UUID_LEN_16,
                .uuid.uuid16 = GATTS_PASSKEY_CHAR_UUID,
            }, ESP_GATT_PERM_WRITE,
            ESP_GATT_CHAR_PROP_BIT_WRITE,
            &(esp_attr_value_t){
                .attr_max_len = 6,  // 6-digit passkey as string
                .attr_len = 0,
                .attr_value = NULL,
            }, NULL);
        } else if (param->add_char.char_uuid.uuid.uuid16 == GATTS_PASSKEY_CHAR_UUID) {
            passkey_char_handle = param->add_char.attr_handle;
            ESP_LOGI(GATTS_TAG, "Passkey change characteristic added");
            
            // Add schedule characteristic (read/write: day(1byte) + 24 chars for write, long read for full schedule)
            esp_ble_gatts_add_char(service_handle, &(esp_bt_uuid_t){
                .len = ESP_UUID_LEN_16,
                .uuid.uuid16 = GATTS_SCHEDULE_CHAR_UUID,
            }, ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
            ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_WRITE,
            &(esp_attr_value_t){
                .attr_max_len = 200,  // Full schedule buffer size
                .attr_len = 0,
                .attr_value = NULL,
            }, NULL);
        } else if (param->add_char.char_uuid.uuid.uuid16 == GATTS_SCHEDULE_CHAR_UUID) {
            schedule_char_handle = param->add_char.attr_handle;
            ESP_LOGI(GATTS_TAG, "Schedule characteristic added");
            
            // Add temperature thresholds characteristic (read/write: 4 bytes)
            esp_ble_gatts_add_char(service_handle, &(esp_bt_uuid_t){
                .len = ESP_UUID_LEN_16,
                .uuid.uuid16 = GATTS_TEMP_CHAR_UUID,
            }, ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
            ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_WRITE,
            &(esp_attr_value_t){
                .attr_max_len = 4,  // 2 bytes high_temp + 2 bytes low_temp
                .attr_len = 0,
                .attr_value = NULL,
            }, NULL);
        } else if (param->add_char.char_uuid.uuid.uuid16 == GATTS_TEMP_CHAR_UUID) {
            temp_char_handle = param->add_char.attr_handle;
            ESP_LOGI(GATTS_TAG, "Temperature thresholds characteristic added");
            
            // Add temperature calibration characteristic (write: 2 bytes signed temp in °C)
            esp_ble_gatts_add_char(service_handle, &(esp_bt_uuid_t){
                .len = ESP_UUID_LEN_16,
                .uuid.uuid16 = GATTS_TEMP_CAL_CHAR_UUID,
            }, ESP_GATT_PERM_WRITE,
            ESP_GATT_CHAR_PROP_BIT_WRITE,
            &(esp_attr_value_t){
                .attr_max_len = 2,  // 2 bytes for temperature in °C
                .attr_len = 0,
                .attr_value = NULL,
            }, NULL);
        } else if (param->add_char.char_uuid.uuid.uuid16 == GATTS_TEMP_CAL_CHAR_UUID) {
            temp_cal_char_handle = param->add_char.attr_handle;
            ESP_LOGI(GATTS_TAG, "Temperature calibration characteristic added");
            
            // Add battery calibration characteristic (write: 2 bytes voltage in mV)
            esp_ble_gatts_add_char(service_handle, &(esp_bt_uuid_t){
                .len = ESP_UUID_LEN_16,
                .uuid.uuid16 = GATTS_BATT_CAL_CHAR_UUID,
            }, ESP_GATT_PERM_WRITE,
            ESP_GATT_CHAR_PROP_BIT_WRITE,
            &(esp_attr_value_t){
                .attr_max_len = 2,  // 2 bytes for voltage in mV
                .attr_len = 0,
                .attr_value = NULL,
            }, NULL);
        } else if (param->add_char.char_uuid.uuid.uuid16 == GATTS_BATT_CAL_CHAR_UUID) {
            batt_cal_char_handle = param->add_char.attr_handle;
            ESP_LOGI(GATTS_TAG, "Battery calibration characteristic added (0xFF0B)");
            
            // Add WiFi SSID characteristic (read/write)
            esp_ble_gatts_add_char(service_handle, &(esp_bt_uuid_t){
                .len = ESP_UUID_LEN_16,
                .uuid.uuid16 = GATTS_WIFI_SSID_CHAR_UUID,
            }, ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
            ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_WRITE,
            &(esp_attr_value_t){
                .attr_max_len = 32,  // Max SSID length
                .attr_len = 0,
                .attr_value = NULL,
            }, NULL);
        } else if (param->add_char.char_uuid.uuid.uuid16 == GATTS_WIFI_SSID_CHAR_UUID) {
            wifi_ssid_char_handle = param->add_char.attr_handle;
            ESP_LOGI(GATTS_TAG, "WiFi SSID characteristic added (0xFF08)");
            
            // Add WiFi Password characteristic (write only)
            esp_ble_gatts_add_char(service_handle, &(esp_bt_uuid_t){
                .len = ESP_UUID_LEN_16,
                .uuid.uuid16 = GATTS_WIFI_PASS_CHAR_UUID,
            }, ESP_GATT_PERM_WRITE,
            ESP_GATT_CHAR_PROP_BIT_WRITE,
            &(esp_attr_value_t){
                .attr_max_len = 64,  // Max WiFi password length
                .attr_len = 0,
                .attr_value = NULL,
            }, NULL);
        } else if (param->add_char.char_uuid.uuid.uuid16 == GATTS_WIFI_PASS_CHAR_UUID) {
            wifi_pass_char_handle = param->add_char.attr_handle;
            ESP_LOGI(GATTS_TAG, "WiFi Password characteristic added (0xFF09)");
            
            // Add BLE Passkey characteristic (write only)
            esp_ble_gatts_add_char(service_handle, &(esp_bt_uuid_t){
                .len = ESP_UUID_LEN_16,
                .uuid.uuid16 = GATTS_BLE_PASSKEY_CHAR_UUID,
            }, ESP_GATT_PERM_WRITE,
            ESP_GATT_CHAR_PROP_BIT_WRITE,
            &(esp_attr_value_t){
                .attr_max_len = 6,  // 6-digit passkey
                .attr_len = 0,
                .attr_value = NULL,
            }, NULL);
        } else if (param->add_char.char_uuid.uuid.uuid16 == GATTS_BLE_PASSKEY_CHAR_UUID) {
            ble_passkey_char_handle = param->add_char.attr_handle;
            ESP_LOGI(GATTS_TAG, "BLE Passkey characteristic added (0xFF0A)");
            
            // Add standard temperature characteristic (Environmental Sensing)
            // Note: NOTIFY property removed since notifications are not implemented
            esp_ble_gatts_add_char(service_handle, &(esp_bt_uuid_t){
                .len = ESP_UUID_LEN_16,
                .uuid.uuid16 = STD_TEMPERATURE_CHAR_UUID,  // 0x2A6E
            }, ESP_GATT_PERM_READ,
            ESP_GATT_CHAR_PROP_BIT_READ,
            &(esp_attr_value_t){
                .attr_max_len = 2,  // 2 bytes for temperature
                .attr_len = 0,
                .attr_value = NULL,
            }, NULL);
        } else if (param->add_char.char_uuid.uuid.uuid16 == STD_TEMPERATURE_CHAR_UUID) {
            std_temp_char_handle = param->add_char.attr_handle;
            ESP_LOGI(GATTS_TAG, "Standard temperature characteristic added (0x2A6E)");
            
            // Add standard current time characteristic
            // Note: NOTIFY property removed since notifications are not implemented
            esp_ble_gatts_add_char(service_handle, &(esp_bt_uuid_t){
                .len = ESP_UUID_LEN_16,
                .uuid.uuid16 = STD_CURRENT_TIME_CHAR_UUID,  // 0x2A2B
            }, ESP_GATT_PERM_READ,
            ESP_GATT_CHAR_PROP_BIT_READ,
            &(esp_attr_value_t){
                .attr_max_len = 10,  // 10 bytes for current time
                .attr_len = 0,
                .attr_value = NULL,
            }, NULL);
        } else if (param->add_char.char_uuid.uuid.uuid16 == STD_CURRENT_TIME_CHAR_UUID) {
            std_time_char_handle = param->add_char.attr_handle;
            ESP_LOGI(GATTS_TAG, "Standard current time characteristic added (0x2A2B)");
            
            // Add manufacturer name characteristic
            esp_ble_gatts_add_char(service_handle, &(esp_bt_uuid_t){
                .len = ESP_UUID_LEN_16,
                .uuid.uuid16 = STD_MANUFACTURER_NAME_UUID,  // 0x2A29
            }, ESP_GATT_PERM_READ,
            ESP_GATT_CHAR_PROP_BIT_READ,
            &(esp_attr_value_t){
                .attr_max_len = 32,
                .attr_len = strlen(MANUFACTURER_NAME),
                .attr_value = (uint8_t *)MANUFACTURER_NAME,
            }, NULL);
        } else if (param->add_char.char_uuid.uuid.uuid16 == STD_MANUFACTURER_NAME_UUID) {
            std_manufacturer_char_handle = param->add_char.attr_handle;
            ESP_LOGI(GATTS_TAG, "Manufacturer name characteristic added (0x2A29)");
            
            // Add model number characteristic
            esp_ble_gatts_add_char(service_handle, &(esp_bt_uuid_t){
                .len = ESP_UUID_LEN_16,
                .uuid.uuid16 = STD_MODEL_NUMBER_UUID,  // 0x2A24
            }, ESP_GATT_PERM_READ,
            ESP_GATT_CHAR_PROP_BIT_READ,
            &(esp_attr_value_t){
                .attr_max_len = 32,
                .attr_len = strlen(MODEL_NUMBER),
                .attr_value = (uint8_t *)MODEL_NUMBER,
            }, NULL);
        } else if (param->add_char.char_uuid.uuid.uuid16 == STD_MODEL_NUMBER_UUID) {
            std_model_char_handle = param->add_char.attr_handle;
            ESP_LOGI(GATTS_TAG, "Model number characteristic added (0x2A24)");
            
            // Add firmware revision characteristic
            esp_ble_gatts_add_char(service_handle, &(esp_bt_uuid_t){
                .len = ESP_UUID_LEN_16,
                .uuid.uuid16 = STD_FIRMWARE_REV_UUID,  // 0x2A26
            }, ESP_GATT_PERM_READ,
            ESP_GATT_CHAR_PROP_BIT_READ,
            &(esp_attr_value_t){
                .attr_max_len = 32,
                .attr_len = strlen(FIRMWARE_REVISION),
                .attr_value = (uint8_t *)FIRMWARE_REVISION,
            }, NULL);
        } else if (param->add_char.char_uuid.uuid.uuid16 == STD_FIRMWARE_REV_UUID) {
            std_firmware_char_handle = param->add_char.attr_handle;
            ESP_LOGI(GATTS_TAG, "Firmware revision characteristic added (0x2A26)");
            
            // Now create the Battery Service (standard service)
            ESP_LOGI(GATTS_TAG, "Creating Battery Service (0x180F)");
            esp_ble_gatts_create_service(gatts_if, &(esp_gatt_srvc_id_t){
                .is_primary = true,
                .id.inst_id = 0x00,
                .id.uuid.len = ESP_UUID_LEN_16,
                .id.uuid.uuid.uuid16 = BATTERY_SERVICE_UUID,
            }, BATTERY_NUM_HANDLE);
        } else if (param->add_char.char_uuid.uuid.uuid16 == BATTERY_LEVEL_CHAR_UUID) {
            battery_level_char_handle = param->add_char.attr_handle;
            ESP_LOGI(GATTS_TAG, "Battery Level characteristic added (0x2A19)");
        }
        break;
        
    case ESP_GATTS_READ_EVT:
        ESP_LOGI(GATTS_TAG, "GATT read request, conn_id %d, trans_id %lu, handle %d", 
                 param->read.conn_id, param->read.trans_id, param->read.handle);
        
        // Log which characteristic is being read
        if (param->read.handle == relay_char_handle) {
            ESP_LOGI(GATTS_TAG, "  -> Reading RELAY_STATE (0xFF02)");
        } else if (param->read.handle == temp_char_handle) {
            ESP_LOGI(GATTS_TAG, "  -> Reading TEMP_THRESHOLDS (0xFF06)");
        } else if (param->read.handle == std_temp_char_handle) {
            ESP_LOGI(GATTS_TAG, "  -> Reading STD_TEMPERATURE (0x2A6E)");
        } else if (param->read.handle == std_time_char_handle) {
            ESP_LOGI(GATTS_TAG, "  -> Reading STD_CURRENT_TIME (0x2A2B)");
        } else if (param->read.handle == battery_level_char_handle) {
            ESP_LOGI(GATTS_TAG, "  -> Reading BATTERY_LEVEL (0x2A19)");
        } else if (param->read.handle == schedule_char_handle) {
            ESP_LOGI(GATTS_TAG, "  -> Reading SCHEDULE (0xFF05)");
        } else if (param->read.handle == wifi_ssid_char_handle) {
            ESP_LOGI(GATTS_TAG, "  -> Reading WIFI_SSID (0xFF08)");
        } else if (param->read.handle == passkey_char_handle) {
            ESP_LOGE(GATTS_TAG, "  -> INVALID: Attempting to read PASSKEY (0xFF04) - WRITE ONLY!");
        } else if (param->read.handle == temp_cal_char_handle) {
            ESP_LOGE(GATTS_TAG, "  -> INVALID: Attempting to read TEMP_CALIBRATION (0xFF07) - WRITE ONLY!");
        } else if (param->read.handle == batt_cal_char_handle) {
            ESP_LOGE(GATTS_TAG, "  -> INVALID: Attempting to read BATTERY_CALIBRATION (0xFF0B) - WRITE ONLY!");
        } else if (param->read.handle == wifi_pass_char_handle) {
            ESP_LOGE(GATTS_TAG, "  -> INVALID: Attempting to read WIFI_PASSWORD (0xFF09) - WRITE ONLY!");
        } else if (param->read.handle == ble_passkey_char_handle) {
            ESP_LOGE(GATTS_TAG, "  -> INVALID: Attempting to read BLE_PASSKEY (0xFF0A) - WRITE ONLY!");
        } else if (param->read.handle == std_manufacturer_char_handle) {
            ESP_LOGI(GATTS_TAG, "  -> Reading MANUFACTURER_NAME (0x2A29)");
        } else if (param->read.handle == std_model_char_handle) {
            ESP_LOGI(GATTS_TAG, "  -> Reading MODEL_NUMBER (0x2A24)");
        } else if (param->read.handle == std_firmware_char_handle) {
            ESP_LOGI(GATTS_TAG, "  -> Reading FIRMWARE_REVISION (0x2A26)");
        } else {
            ESP_LOGW(GATTS_TAG, "  -> UNKNOWN handle %d", param->read.handle);
        }
        
        esp_gatt_rsp_t rsp;
        memset(&rsp, 0, sizeof(esp_gatt_rsp_t));
        rsp.attr_value.handle = param->read.handle;
        
        if (param->read.handle == relay_char_handle) {
            // Return format: "x,hh:mm" where x=relay state, hh:mm=override end time
            // If no override active, just return the relay state "x"
            uint8_t relay_state = gpio_get_relay();
            time_t override_endtime = gpio_get_manual_override_endtime();
            
            if (override_endtime > 0) {
                // Manual override active - format with end time
                struct tm timeinfo;
                localtime_r(&override_endtime, &timeinfo);
                
                char response[16];
                snprintf(response, sizeof(response), "%d,%02d:%02d", 
                         relay_state, timeinfo.tm_hour, timeinfo.tm_min);
                
                rsp.attr_value.len = strlen(response);
                memcpy(rsp.attr_value.value, response, rsp.attr_value.len);
                ESP_LOGI(GATTS_TAG, "Sent relay state with override: %s", response);
            } else {
                // No override - just return state
                char response[8];
                snprintf(response, sizeof(response), "%d", relay_state);
                
                rsp.attr_value.len = strlen(response);
                memcpy(rsp.attr_value.value, response, rsp.attr_value.len);
                ESP_LOGI(GATTS_TAG, "Sent relay state (no override): %s", response);
            }
        } else if (param->read.handle == temp_char_handle) {
            // Read temperature thresholds
            scheduler_config_t config;
            scheduler_get_config(&config);
            
            // Send as 4 bytes: high_temp (2 bytes) + low_temp (2 bytes)
            rsp.attr_value.len = 4;
            rsp.attr_value.value[0] = config.high_temp & 0xFF;
            rsp.attr_value.value[1] = (config.high_temp >> 8) & 0xFF;
            rsp.attr_value.value[2] = config.low_temp & 0xFF;
            rsp.attr_value.value[3] = (config.low_temp >> 8) & 0xFF;
            ESP_LOGI(GATTS_TAG, "Sent temperature thresholds: High=%d°C, Low=%d°C", 
                     config.high_temp, config.low_temp);
        } else if (param->read.handle == std_temp_char_handle) {
            // Standard temperature format: 2 bytes, signed 16-bit, 0.01°C resolution
            int16_t temp_celsius = gpio_read_temperature();
            int16_t temp_value = temp_celsius * 100;  // Convert to 0.01°C units
            
            rsp.attr_value.len = 2;
            rsp.attr_value.value[0] = temp_value & 0xFF;
            rsp.attr_value.value[1] = (temp_value >> 8) & 0xFF;
            ESP_LOGI(GATTS_TAG, "Sent standard temperature: %d.%02d°C", 
                     temp_celsius, abs(temp_value % 100));
        } else if (param->read.handle == std_time_char_handle) {
            // Standard Current Time format: 10 bytes
            time_t now;
            struct tm timeinfo;
            
            time(&now);
            localtime_r(&now, &timeinfo);
            
            uint16_t year = timeinfo.tm_year + 1900;
            rsp.attr_value.value[0] = year & 0xFF;
            rsp.attr_value.value[1] = (year >> 8) & 0xFF;
            rsp.attr_value.value[2] = timeinfo.tm_mon + 1;   // Month (1-12)
            rsp.attr_value.value[3] = timeinfo.tm_mday;      // Day (1-31)
            rsp.attr_value.value[4] = timeinfo.tm_hour;      // Hours (0-23)
            rsp.attr_value.value[5] = timeinfo.tm_min;       // Minutes (0-59)
            rsp.attr_value.value[6] = timeinfo.tm_sec;       // Seconds (0-59)
            rsp.attr_value.value[7] = (timeinfo.tm_wday == 0) ? 7 : timeinfo.tm_wday;  // Day of week
            rsp.attr_value.value[8] = 0;  // Fractions256
            rsp.attr_value.value[9] = 0;  // Adjust reason
            rsp.attr_value.len = 10;
            
            ESP_LOGI(GATTS_TAG, "Sent standard time: %04d-%02d-%02d %02d:%02d:%02d", 
                     year, timeinfo.tm_mon + 1, timeinfo.tm_mday,
                     timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
        } else if (param->read.handle == std_manufacturer_char_handle) {
            // Manufacturer name
            rsp.attr_value.len = strlen(MANUFACTURER_NAME);
            memcpy(rsp.attr_value.value, MANUFACTURER_NAME, rsp.attr_value.len);
            ESP_LOGI(GATTS_TAG, "Sent manufacturer name: %s", MANUFACTURER_NAME);
        } else if (param->read.handle == std_model_char_handle) {
            // Model number
            rsp.attr_value.len = strlen(MODEL_NUMBER);
            memcpy(rsp.attr_value.value, MODEL_NUMBER, rsp.attr_value.len);
            ESP_LOGI(GATTS_TAG, "Sent model number: %s", MODEL_NUMBER);
        } else if (param->read.handle == std_firmware_char_handle) {
            // Firmware revision
            rsp.attr_value.len = strlen(FIRMWARE_REVISION);
            memcpy(rsp.attr_value.value, FIRMWARE_REVISION, rsp.attr_value.len);
            ESP_LOGI(GATTS_TAG, "Sent firmware revision: %s", FIRMWARE_REVISION);
        } else if (param->read.handle == battery_level_char_handle) {
            // Battery level (percentage 0-100)
            uint8_t battery_percentage = gpio_get_battery_percentage();
            uint16_t battery_voltage = gpio_read_battery_voltage();
            
            rsp.attr_value.len = 1;
            rsp.attr_value.value[0] = battery_percentage;
            ESP_LOGI(GATTS_TAG, "Sent battery level: %d%% (%d mV)", battery_percentage, battery_voltage);
        } else if (param->read.handle == schedule_char_handle) {
            // Schedule read - return chunks based on internal offset
            // Client must write 0xFF first to reset offset and prepare full schedule
            
            uint16_t total_len = schedule_buffer_valid ? strlen(schedule_read_buffer) : 0;
            
            ESP_LOGI(GATTS_TAG, "Schedule read request: offset=%d, total=%d, valid=%d", 
                     schedule_read_offset, total_len, schedule_buffer_valid);
            
            if (!schedule_buffer_valid || schedule_read_offset >= total_len) {
                // No data or read complete, return empty
                rsp.attr_value.len = 0;
                ESP_LOGI(GATTS_TAG, "Schedule read complete or not initialized");
            } else {
                // Return next chunk
                uint16_t remaining = total_len - schedule_read_offset;
                uint16_t chunk_size = (remaining > GATTS_CHUNK_SIZE) ? GATTS_CHUNK_SIZE : remaining;
                
                memcpy(rsp.attr_value.value, &schedule_read_buffer[schedule_read_offset], chunk_size);
                rsp.attr_value.len = chunk_size;
                
                ESP_LOGI(GATTS_TAG, "Sending schedule chunk: offset=%d, len=%d, remaining=%d, total=%d", 
                         schedule_read_offset, chunk_size, remaining - chunk_size, total_len);
                
                // Advance offset for next read
                schedule_read_offset += chunk_size;
            }
        } else if (param->read.handle == wifi_ssid_char_handle) {
            // Read WiFi SSID from NVS
            char ssid[33] = {0};
            load_wifi_ssid_from_nvs(ssid, sizeof(ssid));
            
            rsp.attr_value.len = strlen(ssid);
            memcpy(rsp.attr_value.value, ssid, rsp.attr_value.len);
            ESP_LOGI(GATTS_TAG, "Sent WiFi SSID: %s", ssid);
        } else {
            // Unhandled characteristic - this will cause GATT_READ_NOT_PERMIT
            ESP_LOGE(GATTS_TAG, "  -> ERROR: Unhandled read for handle %d - no response prepared!", param->read.handle);
            ESP_LOGE(GATTS_TAG, "  -> Known handles: relay=%d, temp_thresh=%d, std_temp=%d, std_time=%d, battery=%d, schedule=%d, wifi_ssid=%d",
                     relay_char_handle, temp_char_handle, std_temp_char_handle, std_time_char_handle,
                     battery_level_char_handle, schedule_char_handle, wifi_ssid_char_handle);
            ESP_LOGE(GATTS_TAG, "  -> Write-only handles: passkey=%d, temp_cal=%d, batt_cal=%d, wifi_pass=%d, ble_passkey=%d",
                     passkey_char_handle, temp_cal_char_handle, batt_cal_char_handle, 
                     wifi_pass_char_handle, ble_passkey_char_handle);
            // Send error response
            esp_ble_gatts_send_response(gatts_if, param->read.conn_id, param->read.trans_id, ESP_GATT_READ_NOT_PERMIT, &rsp);
            break;
        }
        
        esp_ble_gatts_send_response(gatts_if, param->read.conn_id, param->read.trans_id, ESP_GATT_OK, &rsp);
        break;
        
    case ESP_GATTS_WRITE_EVT:
        ESP_LOGI(GATTS_TAG, "GATT write request, conn_id %d, trans_id %lu, handle %d, len %d, value[0] %d", 
                 param->write.conn_id, param->write.trans_id, param->write.handle,
                 param->write.len, param->write.len > 0 ? param->write.value[0] : 0);
        
        if (param->write.handle == relay_char_handle) {
            // Relay write format: "x,y" where x=state (0/1), y=duration in 15-minute units
            // Example: "0,4" = relay OFF for 60 minutes (4 * 15)
            if (param->write.len >= 3) {
                // Parse format: "x,y"
                char buffer[16];
                size_t len = (param->write.len < sizeof(buffer) - 1) ? param->write.len : sizeof(buffer) - 1;
                memcpy(buffer, param->write.value, len);
                buffer[len] = '\0';
                
                uint8_t relay_state = 0;
                uint16_t duration_units = 0;
                
                if (sscanf(buffer, "%hhu,%hu", &relay_state, &duration_units) == 2) {
                    // Convert 15-minute units to minutes
                    uint16_t duration_minutes = duration_units * 15;
                    
                    if (relay_state <= 1 && duration_minutes > 0 && duration_minutes <= 1440) {  // Max 24 hours
                        gpio_set_relay_manual(relay_state, duration_minutes);
                        ESP_LOGI(GATTS_TAG, "Relay manual override: state=%d, duration=%d min (%d units)", 
                                 relay_state, duration_minutes, duration_units);
                        
                        if (param->write.need_rsp) {
                            esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, ESP_GATT_OK, NULL);
                        }
                    } else {
                        ESP_LOGW(GATTS_TAG, "Invalid relay parameters: state=%d, duration=%d", relay_state, duration_minutes);
                        if (param->write.need_rsp) {
                            esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, 
                                                       ESP_GATT_INVALID_ATTR_LEN, NULL);
                        }
                    }
                } else {
                    ESP_LOGW(GATTS_TAG, "Failed to parse relay command: %s", buffer);
                    if (param->write.need_rsp) {
                        esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, 
                                                   ESP_GATT_INVALID_ATTR_LEN, NULL);
                    }
                }
            } else {
                ESP_LOGW(GATTS_TAG, "Invalid relay write length: %d (expected >= 3)", param->write.len);
                if (param->write.need_rsp) {
                    esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, 
                                               ESP_GATT_INVALID_ATTR_LEN, NULL);
                }
            }
        } else if (param->write.handle == passkey_char_handle) {
            // Passkey change functionality (kept for future use)
            if (param->write.len == 6) {
                char passkey_str[7];
                memcpy(passkey_str, param->write.value, 6);
                passkey_str[6] = '\0';
                
                uint32_t new_passkey = atoi(passkey_str);
                if (new_passkey <= 999999) {
                    current_passkey = new_passkey;
                    save_passkey_to_nvs(new_passkey);
                    ESP_LOGI(GATTS_TAG, "Passkey changed to: %lu", (unsigned long)new_passkey);
                    
                    if (param->write.need_rsp) {
                        esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, ESP_GATT_OK, NULL);
                    }
                } else {
                    ESP_LOGW(GATTS_TAG, "Invalid passkey format");
                    if (param->write.need_rsp) {
                        esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, 
                                                   ESP_GATT_INVALID_ATTR_LEN, NULL);
                    }
                }
            } else {
                ESP_LOGW(GATTS_TAG, "Invalid passkey length: %d (expected 6)", param->write.len);
                if (param->write.need_rsp) {
                    esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, 
                                               ESP_GATT_INVALID_ATTR_LEN, NULL);
                }
            }
        } else if (param->write.handle == schedule_char_handle) {
            // Schedule write: 
            // - 1 byte 0xFF = request full schedule read (resets offset)
            // - 1 byte day (0-6) + 24 chars schedule = update day schedule
            if (param->write.len == 1 && param->write.value[0] == 0xFF) {
                // Request to read full schedule - build buffer and reset offset
                build_schedule_read_buffer();
                schedule_read_offset = 0;
                ESP_LOGI(GATTS_TAG, "Schedule read requested, buffer prepared, offset reset");
                
                if (param->write.need_rsp) {
                    esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, ESP_GATT_OK, NULL);
                }
            } else if (param->write.len == 25) {
                // Update day schedule: 1 byte day + 24 chars
                uint8_t day = param->write.value[0];
                char schedule[25];
                memcpy(schedule, &param->write.value[1], 24);
                schedule[24] = '\0';
                
                if (scheduler_set_day_schedule(day, schedule) == ESP_OK) {
                    scheduler_save_config();
                    schedule_buffer_valid = false;  // Invalidate buffer so it gets rebuilt on next read
                    ESP_LOGI(GATTS_TAG, "Schedule updated for day %d: %s", day, schedule);
                    if (param->write.need_rsp) {
                        esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, ESP_GATT_OK, NULL);
                    }
                } else {
                    ESP_LOGW(GATTS_TAG, "Invalid schedule data");
                    if (param->write.need_rsp) {
                        esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, 
                                                   ESP_GATT_INVALID_ATTR_LEN, NULL);
                    }
                }
            } else {
                ESP_LOGW(GATTS_TAG, "Invalid schedule write length: %d (expected 1 or 25)", param->write.len);
                if (param->write.need_rsp) {
                    esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, 
                                               ESP_GATT_INVALID_ATTR_LEN, NULL);
                }
            }
        } else if (param->write.handle == temp_char_handle) {
            // Temperature thresholds write: 4 bytes (high_temp + low_temp)
            if (param->write.len == 4) {
                int16_t high_temp = (int16_t)(param->write.value[0] | (param->write.value[1] << 8));
                int16_t low_temp = (int16_t)(param->write.value[2] | (param->write.value[3] << 8));
                
                if (scheduler_set_temperatures(high_temp, low_temp) == ESP_OK) {
                    scheduler_save_config();
                    ESP_LOGI(GATTS_TAG, "Temperature thresholds updated: High=%d°C, Low=%d°C", 
                             high_temp, low_temp);
                    if (param->write.need_rsp) {
                        esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, ESP_GATT_OK, NULL);
                    }
                } else {
                    ESP_LOGW(GATTS_TAG, "Invalid temperature values");
                    if (param->write.need_rsp) {
                        esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, 
                                                   ESP_GATT_INVALID_ATTR_LEN, NULL);
                    }
                }
            } else {
                ESP_LOGW(GATTS_TAG, "Invalid temperature data length: %d (expected 4)", param->write.len);
                if (param->write.need_rsp) {
                    esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, 
                                               ESP_GATT_INVALID_ATTR_LEN, NULL);
                }
            }
        } else if (param->write.handle == temp_cal_char_handle) {
            // Temperature calibration write: 2 bytes (signed 16-bit temperature in °C)
            if (param->write.len == 2) {
                int16_t actual_temp = (int16_t)(param->write.value[0] | (param->write.value[1] << 8));
                
                // Special value -999 to reset calibration
                if (actual_temp == -999) {
                    if (gpio_reset_temperature_calibration() == ESP_OK) {
                        ESP_LOGI(GATTS_TAG, "Temperature calibration reset to factory defaults");
                        if (param->write.need_rsp) {
                            esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, ESP_GATT_OK, NULL);
                        }
                    } else {
                        ESP_LOGW(GATTS_TAG, "Temperature calibration reset failed");
                        if (param->write.need_rsp) {
                            esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, 
                                                       ESP_GATT_ERROR, NULL);
                        }
                    }
                } else if (gpio_calibrate_temperature(actual_temp) == ESP_OK) {
                    ESP_LOGI(GATTS_TAG, "Temperature calibration point added: %d°C", actual_temp);
                    if (param->write.need_rsp) {
                        esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, ESP_GATT_OK, NULL);
                    }
                } else {
                    ESP_LOGW(GATTS_TAG, "Temperature calibration failed");
                    if (param->write.need_rsp) {
                        esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, 
                                                   ESP_GATT_ERROR, NULL);
                    }
                }
            } else {
                ESP_LOGW(GATTS_TAG, "Invalid calibration data length: %d (expected 2)", param->write.len);
                if (param->write.need_rsp) {
                    esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, 
                                               ESP_GATT_INVALID_ATTR_LEN, NULL);
                }
            }
        } else if (param->write.handle == batt_cal_char_handle) {
            // Battery calibration write: 2 bytes (unsigned 16-bit voltage in mV)
            if (param->write.len == 2) {
                uint16_t actual_voltage_mv = (uint16_t)(param->write.value[0] | (param->write.value[1] << 8));
                
                // Special value 65535 (0xFFFF) to reset calibration
                if (actual_voltage_mv == 65535) {
                    if (gpio_reset_battery_calibration() == ESP_OK) {
                        ESP_LOGI(GATTS_TAG, "Battery calibration reset to factory defaults");
                        if (param->write.need_rsp) {
                            esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, ESP_GATT_OK, NULL);
                        }
                    } else {
                        ESP_LOGW(GATTS_TAG, "Battery calibration reset failed");
                        if (param->write.need_rsp) {
                            esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, 
                                                       ESP_GATT_ERROR, NULL);
                        }
                    }
                } else if (actual_voltage_mv >= 2500 && actual_voltage_mv <= 5000) {
                    if (gpio_calibrate_battery_voltage(actual_voltage_mv) == ESP_OK) {
                        ESP_LOGI(GATTS_TAG, "Battery calibration point added: %dmV", actual_voltage_mv);
                        if (param->write.need_rsp) {
                            esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, ESP_GATT_OK, NULL);
                        }
                    } else {
                        ESP_LOGW(GATTS_TAG, "Battery calibration failed");
                        if (param->write.need_rsp) {
                            esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, 
                                                       ESP_GATT_ERROR, NULL);
                        }
                    }
                } else {
                    ESP_LOGW(GATTS_TAG, "Invalid battery voltage: %dmV (must be 2500-5000mV or 65535 for reset)", actual_voltage_mv);
                    if (param->write.need_rsp) {
                        esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, 
                                                   ESP_GATT_INVALID_PDU, NULL);
                    }
                }
            } else {
                ESP_LOGW(GATTS_TAG, "Invalid battery calibration data length: %d (expected 2)", param->write.len);
                if (param->write.need_rsp) {
                    esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, 
                                               ESP_GATT_INVALID_ATTR_LEN, NULL);
                }
            }
        } else if (param->write.handle == wifi_ssid_char_handle) {
            // WiFi SSID write: string up to 32 bytes
            if (param->write.len > 0 && param->write.len <= 32) {
                char ssid[33] = {0};
                memcpy(ssid, param->write.value, param->write.len);
                ssid[param->write.len] = '\0';
                
                if (save_wifi_ssid_to_nvs(ssid) == ESP_OK) {
                    ESP_LOGI(GATTS_TAG, "WiFi SSID updated: %s", ssid);
                    if (param->write.need_rsp) {
                        esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, ESP_GATT_OK, NULL);
                    }
                } else {
                    ESP_LOGW(GATTS_TAG, "Failed to save WiFi SSID");
                    if (param->write.need_rsp) {
                        esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, 
                                                   ESP_GATT_ERROR, NULL);
                    }
                }
            } else {
                ESP_LOGW(GATTS_TAG, "Invalid WiFi SSID length: %d (expected 1-32)", param->write.len);
                if (param->write.need_rsp) {
                    esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, 
                                               ESP_GATT_INVALID_ATTR_LEN, NULL);
                }
            }
        } else if (param->write.handle == wifi_pass_char_handle) {
            // WiFi Password write: string up to 64 bytes (write only, logged for debugging)
            if (param->write.len > 0 && param->write.len <= 64) {
                char password[65] = {0};
                memcpy(password, param->write.value, param->write.len);
                password[param->write.len] = '\0';
                
                if (save_wifi_password_to_nvs(password) == ESP_OK) {
                    ESP_LOGI(GATTS_TAG, "WiFi Password updated: %s", password);
                    if (param->write.need_rsp) {
                        esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, ESP_GATT_OK, NULL);
                    }
                } else {
                    ESP_LOGW(GATTS_TAG, "Failed to save WiFi password");
                    if (param->write.need_rsp) {
                        esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, 
                                                   ESP_GATT_ERROR, NULL);
                    }
                }
            } else {
                ESP_LOGW(GATTS_TAG, "Invalid WiFi password length: %d (expected 1-64)", param->write.len);
                if (param->write.need_rsp) {
                    esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, 
                                               ESP_GATT_INVALID_ATTR_LEN, NULL);
                }
            }
        } else if (param->write.handle == ble_passkey_char_handle) {
            // BLE Passkey write: 6-digit numeric string (write only, logged for debugging)
            if (param->write.len == 6) {
                char passkey_str[7] = {0};
                memcpy(passkey_str, param->write.value, 6);
                passkey_str[6] = '\0';
                
                // Convert string to uint32_t
                uint32_t new_passkey = 0;
                bool valid = true;
                for (int i = 0; i < 6; i++) {
                    if (passkey_str[i] >= '0' && passkey_str[i] <= '9') {
                        new_passkey = new_passkey * 10 + (passkey_str[i] - '0');
                    } else {
                        valid = false;
                        break;
                    }
                }
                
                if (valid && new_passkey <= 999999) {
                    if (save_passkey_to_nvs(new_passkey) == ESP_OK) {
                        current_passkey = new_passkey;
                        ESP_LOGI(GATTS_TAG, "BLE Passkey updated: %06lu", (unsigned long)new_passkey);
                        if (param->write.need_rsp) {
                            esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, ESP_GATT_OK, NULL);
                        }
                    } else {
                        ESP_LOGW(GATTS_TAG, "Failed to save BLE passkey");
                        if (param->write.need_rsp) {
                            esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, 
                                                       ESP_GATT_ERROR, NULL);
                        }
                    }
                } else {
                    ESP_LOGW(GATTS_TAG, "Invalid BLE passkey format: %s", passkey_str);
                    if (param->write.need_rsp) {
                        esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, 
                                                   ESP_GATT_INVALID_ATTR_LEN, NULL);
                    }
                }
            } else {
                ESP_LOGW(GATTS_TAG, "Invalid BLE passkey length: %d (expected 6)", param->write.len);
                if (param->write.need_rsp) {
                    esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, 
                                               ESP_GATT_INVALID_ATTR_LEN, NULL);
                }
            }
        }
        break;
        
    case ESP_GATTS_CONNECT_EVT:
        ESP_LOGI(GATTS_TAG, "Client connected, conn_id %d", param->connect.conn_id);
        ble_connected = true;
        ble_authenticated = false;  // Wait for authentication to complete
        
        // Set LED to blue - BLE connected
        led_status_set(LED_STATUS_BLE_CONNECTED);
        
        // Stop advertising when connected
        esp_ble_gap_stop_advertising();
        
        // Initiate security pairing process
        ESP_LOGI(GATTS_TAG, "Initiating security pairing...");
        esp_ble_set_encryption(param->connect.remote_bda, ESP_BLE_SEC_ENCRYPT_MITM);
        break;
        
    case ESP_GATTS_DISCONNECT_EVT:
        ESP_LOGI(GATTS_TAG, "Client disconnected");
        ble_connected = false;
        ble_authenticated = false;  // Reset authentication on disconnect
        // Don't restart advertising - we'll go to sleep instead
        break;
        
    default:
        break;
    }
}

esp_err_t ble_server_init(void)
{
    esp_err_t ret;

    // Load passkey from NVS
    load_passkey_from_nvs();
    
    // Initialize scheduler
    ret = scheduler_init();
    if (ret != ESP_OK) {
        ESP_LOGW(GATTS_TAG, "Scheduler init warning: %s", esp_err_to_name(ret));
    }

    // Initialize all GPIOs
    ret = gpio_manager_init();
    if (ret != ESP_OK) {
        ESP_LOGE(GATTS_TAG, "GPIO manager init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ret = esp_bt_controller_init(&bt_cfg);
    if (ret) {
        ESP_LOGE(GATTS_TAG, "Bluetooth controller init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = esp_bt_controller_enable(ESP_BT_MODE_BLE);
    if (ret) {
        ESP_LOGE(GATTS_TAG, "Bluetooth controller enable failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = esp_bluedroid_init();
    if (ret) {
        ESP_LOGE(GATTS_TAG, "Bluedroid init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = esp_bluedroid_enable();
    if (ret) {
        ESP_LOGE(GATTS_TAG, "Bluedroid enable failed: %s", esp_err_to_name(ret));
        return ret;
    }

    // Set BLE security parameters
    // Using encryption, bonding, and MITM protection with passkey display
    // The device will display a passkey that must be confirmed on the connecting device
    esp_ble_auth_req_t auth_req = ESP_LE_AUTH_REQ_SC_MITM_BOND;  // Secure Connections + MITM + Bonding
    esp_ble_io_cap_t iocap = ESP_IO_CAP_OUT;  // Display only (device shows passkey)
    uint8_t key_size = 16;
    uint8_t init_key = ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK;
    uint8_t rsp_key = ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK;
    uint8_t auth_option = ESP_BLE_ONLY_ACCEPT_SPECIFIED_AUTH_DISABLE;
    uint32_t passkey = current_passkey;
    
    esp_ble_gap_set_security_param(ESP_BLE_SM_AUTHEN_REQ_MODE, &auth_req, sizeof(uint8_t));
    esp_ble_gap_set_security_param(ESP_BLE_SM_IOCAP_MODE, &iocap, sizeof(uint8_t));
    esp_ble_gap_set_security_param(ESP_BLE_SM_MAX_KEY_SIZE, &key_size, sizeof(uint8_t));
    esp_ble_gap_set_security_param(ESP_BLE_SM_ONLY_ACCEPT_SPECIFIED_SEC_AUTH, &auth_option, sizeof(uint8_t));
    esp_ble_gap_set_security_param(ESP_BLE_SM_SET_INIT_KEY, &init_key, sizeof(uint8_t));
    esp_ble_gap_set_security_param(ESP_BLE_SM_SET_RSP_KEY, &rsp_key, sizeof(uint8_t));
    esp_ble_gap_set_security_param(ESP_BLE_SM_SET_STATIC_PASSKEY, &passkey, sizeof(uint32_t));
    
    ESP_LOGI(GATTS_TAG, "BLE security configured: Auth=Bond+MITM+Encryption (passkey: %06lu)", (unsigned long)current_passkey);

    ret = esp_ble_gap_register_callback(gap_event_handler);
    if (ret) {
        ESP_LOGE(GATTS_TAG, "GAP register callback failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = esp_ble_gatts_register_callback(gatts_event_handler);
    if (ret) {
        ESP_LOGE(GATTS_TAG, "GATTS register callback failed: %s", esp_err_to_name(ret));
        return ret;
    }

    // Build raw advertising data with device name
    uint8_t name_len = strlen(DEVICE_NAME);
    uint8_t idx = 0;
    
    // Flags
    raw_adv_data[idx++] = 0x02;  // Length
    raw_adv_data[idx++] = 0x01;  // Type: Flags
    raw_adv_data[idx++] = 0x06;  // General Discoverable, BR/EDR not supported
    
    // Service UUIDs (custom service + battery service)
    raw_adv_data[idx++] = 0x05;  // Length (2 UUIDs * 2 bytes + 1 type byte)
    raw_adv_data[idx++] = 0x03;  // Type: Complete list of 16-bit UUIDs
    raw_adv_data[idx++] = 0xFF;  // Custom service UUID low byte
    raw_adv_data[idx++] = 0x00;  // Custom service UUID high byte
    raw_adv_data[idx++] = 0x0F;  // Battery service UUID low byte
    raw_adv_data[idx++] = 0x18;  // Battery service UUID high byte
    
    // Complete local name
    raw_adv_data[idx++] = name_len + 1;  // Length (name + type byte)
    raw_adv_data[idx++] = 0x09;  // Type: Complete local name
    memcpy(&raw_adv_data[idx], DEVICE_NAME, name_len);
    idx += name_len;
    
    ESP_LOGI(GATTS_TAG, "Built advertising data with device name: %s (total %d bytes)", DEVICE_NAME, idx);

    ret = esp_ble_gatts_app_register(PROFILE_APP_ID);
    if (ret) {
        ESP_LOGE(GATTS_TAG, "GATTS app register failed: %s", esp_err_to_name(ret));
        return ret;
    }

    esp_err_t local_mtu_ret = esp_ble_gatt_set_local_mtu(500);
    if (local_mtu_ret) {
        ESP_LOGE(GATTS_TAG, "Set local MTU failed: %s", esp_err_to_name(local_mtu_ret));
    }

    ESP_LOGI(GATTS_TAG, "BLE server initialized successfully");
    return ESP_OK;
}

void ble_server_deinit(void)
{
    ESP_LOGI(GATTS_TAG, "Deinitializing BLE server");
    
    // Stop advertising if running
    esp_ble_gap_stop_advertising();
    
    // Disable and deinitialize Bluedroid
    esp_bluedroid_disable();
    esp_bluedroid_deinit();
    
    // Disable and deinitialize BT controller
    esp_bt_controller_disable();
    esp_bt_controller_deinit();
    
    ESP_LOGI(GATTS_TAG, "BLE server deinitialized");
}

void ble_update_standard_time(void)
{
    if (std_time_char_handle == 0 || !ble_connected) {
        return;
    }
    
    uint8_t time_data[10];
    time_t now;
    struct tm timeinfo;
    
    time(&now);
    localtime_r(&now, &timeinfo);
    
    uint16_t year = timeinfo.tm_year + 1900;
    time_data[0] = year & 0xFF;
    time_data[1] = (year >> 8) & 0xFF;
    time_data[2] = timeinfo.tm_mon + 1;
    time_data[3] = timeinfo.tm_mday;
    time_data[4] = timeinfo.tm_hour;
    time_data[5] = timeinfo.tm_min;
    time_data[6] = timeinfo.tm_sec;
    time_data[7] = (timeinfo.tm_wday == 0) ? 7 : timeinfo.tm_wday;
    time_data[8] = 0;
    time_data[9] = 0;
    
    // Send notification if client subscribed
    esp_ble_gatts_send_indicate(gatts_if_global, 0, std_time_char_handle, 
                                 10, time_data, false);
}

void ble_update_standard_temperature(void)
{
    if (std_temp_char_handle == 0 || !ble_connected) {
        return;
    }
    
    int16_t temp_celsius = gpio_read_temperature();
    int16_t temp_value = temp_celsius * 100;
    
    uint8_t temp_data[2];
    temp_data[0] = temp_value & 0xFF;
    temp_data[1] = (temp_value >> 8) & 0xFF;
    
    // Send notification if client subscribed
    esp_ble_gatts_send_indicate(gatts_if_global, 0, std_temp_char_handle, 
                                 2, temp_data, false);
}

void ble_update_battery_level(void)
{
    if (battery_level_char_handle == 0 || !ble_connected) {
        return;
    }
    
    uint8_t battery_percentage = gpio_get_battery_percentage();
    uint16_t battery_voltage = gpio_read_battery_voltage();
    
    uint8_t battery_data[1];
    battery_data[0] = battery_percentage;
    
    // Send notification if client subscribed
    esp_ble_gatts_send_indicate(gatts_if_global, 0, battery_level_char_handle, 
                                 1, battery_data, false);
    
    ESP_LOGI(GATTS_TAG, "Battery level updated: %d%% (%d mV)", battery_percentage, battery_voltage);
}

bool ble_is_connected(void)
{
    return ble_connected;
}

uint8_t ble_get_relay_state(void)
{
    return gpio_get_relay();
}

uint16_t ble_get_adc_voltage(void)
{
    return gpio_read_adc_voltage();
}

int16_t ble_get_temperature(void)
{
    return gpio_read_temperature();
}
