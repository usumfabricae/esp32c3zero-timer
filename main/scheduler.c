#include "scheduler.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include <string.h>
#include <time.h>

#define SCHEDULER_TAG "scheduler"

// NVS keys
#define NVS_SCHEDULER_NAMESPACE "scheduler"
#define NVS_HIGH_TEMP_KEY "high_temp"
#define NVS_LOW_TEMP_KEY "low_temp"
#define NVS_SCHEDULE_KEY_PREFIX "day_"  // day_0 to day_6

// Default configuration
#define DEFAULT_HIGH_TEMP 22  // 22°C
#define DEFAULT_LOW_TEMP 18   // 18°C
#define DEFAULT_SCHEDULE "OOOOOOHOOOOOOOOOOOOOOOOO"  // High mode from 6-7, Off rest of day

static scheduler_config_t current_config;

esp_err_t scheduler_init(void)
{
    esp_err_t ret;
    nvs_handle_t nvs_handle;
    
    // Initialize with defaults
    current_config.high_temp = DEFAULT_HIGH_TEMP;
    current_config.low_temp = DEFAULT_LOW_TEMP;
    
    for (int day = 0; day < DAYS_PER_WEEK; day++) {
        strcpy(current_config.schedule[day], DEFAULT_SCHEDULE);
    }
    
    // Try to load from NVS
    ret = nvs_open(NVS_SCHEDULER_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGI(SCHEDULER_TAG, "No saved scheduler config, using defaults");
        return ESP_OK;
    }
    
    // Load temperatures
    int16_t temp;
    if (nvs_get_i16(nvs_handle, NVS_HIGH_TEMP_KEY, &temp) == ESP_OK) {
        current_config.high_temp = temp;
    }
    if (nvs_get_i16(nvs_handle, NVS_LOW_TEMP_KEY, &temp) == ESP_OK) {
        current_config.low_temp = temp;
    }
    
    // Load schedules for each day
    for (int day = 0; day < DAYS_PER_WEEK; day++) {
        char key[16];
        snprintf(key, sizeof(key), "%s%d", NVS_SCHEDULE_KEY_PREFIX, day);
        
        size_t required_size = HOURS_PER_DAY + 1;
        if (nvs_get_str(nvs_handle, key, current_config.schedule[day], &required_size) == ESP_OK) {
            ESP_LOGI(SCHEDULER_TAG, "Loaded schedule for day %d: %s", day, current_config.schedule[day]);
        }
    }
    
    nvs_close(nvs_handle);
    
    ESP_LOGI(SCHEDULER_TAG, "Scheduler initialized: High=%d°C, Low=%d°C", 
             current_config.high_temp, current_config.low_temp);
    
    return ESP_OK;
}

esp_err_t scheduler_get_config(scheduler_config_t *config)
{
    if (config == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    memcpy(config, &current_config, sizeof(scheduler_config_t));
    return ESP_OK;
}

esp_err_t scheduler_set_day_schedule(uint8_t day, const char *schedule)
{
    if (day >= DAYS_PER_WEEK || schedule == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (strlen(schedule) != HOURS_PER_DAY) {
        ESP_LOGE(SCHEDULER_TAG, "Invalid schedule length: %d (expected %d)", 
                 strlen(schedule), HOURS_PER_DAY);
        return ESP_ERR_INVALID_ARG;
    }
    
    // Validate schedule characters
    for (int i = 0; i < HOURS_PER_DAY; i++) {
        if (schedule[i] != SCHEDULE_MODE_HIGH && 
            schedule[i] != SCHEDULE_MODE_LOW && 
            schedule[i] != SCHEDULE_MODE_OFF) {
            ESP_LOGE(SCHEDULER_TAG, "Invalid schedule character at position %d: '%c'", i, schedule[i]);
            return ESP_ERR_INVALID_ARG;
        }
    }
    
    strcpy(current_config.schedule[day], schedule);
    ESP_LOGI(SCHEDULER_TAG, "Schedule set for day %d: %s", day, schedule);
    
    return ESP_OK;
}

esp_err_t scheduler_set_temperatures(int16_t high_temp, int16_t low_temp)
{
    if (high_temp <= low_temp) {
        ESP_LOGE(SCHEDULER_TAG, "High temp (%d) must be greater than low temp (%d)", 
                 high_temp, low_temp);
        return ESP_ERR_INVALID_ARG;
    }
    
    current_config.high_temp = high_temp;
    current_config.low_temp = low_temp;
    
    ESP_LOGI(SCHEDULER_TAG, "Temperatures set: High=%d°C, Low=%d°C", high_temp, low_temp);
    
    return ESP_OK;
}

day_of_week_t scheduler_get_day_of_week(struct tm *timeinfo)
{
    // tm_wday: 0=Sunday, 1=Monday, ..., 6=Saturday
    // Convert to: 0=Monday, 1=Tuesday, ..., 6=Sunday
    int day = timeinfo->tm_wday - 1;
    if (day < 0) {
        day = 6;  // Sunday
    }
    return (day_of_week_t)day;
}

char scheduler_get_current_mode(void)
{
    time_t now;
    struct tm timeinfo;
    
    time(&now);
    
    // Check if time is valid (after year 2024)
    if (now < 1704067200) {  // Jan 1, 2024 00:00:00 UTC
        ESP_LOGW(SCHEDULER_TAG, "Time not synchronized yet, returning OFF mode");
        return SCHEDULE_MODE_OFF;
    }
    
    localtime_r(&now, &timeinfo);
    
    day_of_week_t day = scheduler_get_day_of_week(&timeinfo);
    int hour = timeinfo.tm_hour;
    
    if (hour < 0 || hour >= HOURS_PER_DAY) {
        ESP_LOGW(SCHEDULER_TAG, "Invalid hour: %d", hour);
        return SCHEDULE_MODE_OFF;
    }
    
    char mode = current_config.schedule[day][hour];
    
    ESP_LOGD(SCHEDULER_TAG, "Current mode: day=%d, hour=%d, mode='%c'", day, hour, mode);
    
    return mode;
}

bool scheduler_should_relay_be_on(int16_t current_temp)
{
    char mode = scheduler_get_current_mode();
    int16_t threshold;
    
    switch (mode) {
        case SCHEDULE_MODE_HIGH:
            threshold = current_config.high_temp;
            break;
        case SCHEDULE_MODE_LOW:
            threshold = current_config.low_temp;
            break;
        case SCHEDULE_MODE_OFF:
            ESP_LOGD(SCHEDULER_TAG, "Schedule mode is OFF, relay should be off");
            return false;
        default:
            ESP_LOGW(SCHEDULER_TAG, "Unknown schedule mode: '%c', relay off", mode);
            return false;
    }
    
    bool should_be_on = (current_temp < threshold);
    
    ESP_LOGD(SCHEDULER_TAG, "Temp=%d°C, Threshold=%d°C (mode='%c'), Relay=%s", 
             current_temp, threshold, mode, should_be_on ? "ON" : "OFF");
    
    return should_be_on;
}

esp_err_t scheduler_save_config(void)
{
    nvs_handle_t nvs_handle;
    esp_err_t ret;
    
    ret = nvs_open(NVS_SCHEDULER_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(SCHEDULER_TAG, "Failed to open NVS for writing");
        return ret;
    }
    
    // Save temperatures
    nvs_set_i16(nvs_handle, NVS_HIGH_TEMP_KEY, current_config.high_temp);
    nvs_set_i16(nvs_handle, NVS_LOW_TEMP_KEY, current_config.low_temp);
    
    // Save schedules
    for (int day = 0; day < DAYS_PER_WEEK; day++) {
        char key[16];
        snprintf(key, sizeof(key), "%s%d", NVS_SCHEDULE_KEY_PREFIX, day);
        nvs_set_str(nvs_handle, key, current_config.schedule[day]);
    }
    
    ret = nvs_commit(nvs_handle);
    nvs_close(nvs_handle);
    
    if (ret == ESP_OK) {
        ESP_LOGI(SCHEDULER_TAG, "Configuration saved to NVS");
    } else {
        ESP_LOGE(SCHEDULER_TAG, "Failed to commit NVS");
    }
    
    return ret;
}
