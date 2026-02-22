#include "gpio_manager.h"
#include "config.h"
#include "driver/gpio.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_log.h"
#include "esp_sleep.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <time.h>

#define GPIO_TAG "gpio_manager"

// ADC configuration
#define ADC_GPIO TEMP_SENSOR_GPIO
#define ADC_UNIT ADC_UNIT_1
#define ADC_CHANNEL ADC_CHANNEL_3  // GPIO3 is ADC1 Channel 3 on ESP32-C3
#define ADC_ATTEN ADC_ATTEN_DB_12  // 0-3.3V range

// Battery voltage ADC configuration
#define BATTERY_GPIO BATTERY_VOLTAGE_GPIO
#define BATTERY_ADC_CHANNEL ADC_CHANNEL_2  // GPIO2 is ADC1 Channel 2 on ESP32-C3

// NVS keys for temperature calibration
#define NVS_TEMP_CAL_NAMESPACE "temp_cal"
#define NVS_CAL_COUNT_KEY "cal_count"
#define NVS_CAL_TEMP_PREFIX "cal_temp_"
#define NVS_CAL_MV_PREFIX "cal_mv_"

// NVS key for relay state
#define NVS_NAMESPACE "gpio_state"
#define NVS_RELAY_KEY "relay_state"
#define NVS_MANUAL_OVERRIDE_KEY "manual_ovr"

// Static variables
static uint8_t relay_state = 0;
static adc_oneshot_unit_handle_t adc_handle = NULL;
static adc_cali_handle_t adc_cali_handle = NULL;
static bool adc_calibrated = false;

// Temperature calibration storage (multiple points for linear regression)
typedef struct {
    int16_t temp_celsius;
    uint16_t voltage_mv;
} temp_cal_point_t;

static temp_cal_point_t cal_points[MAX_CALIBRATION_POINTS];
static uint8_t cal_point_count = 0;

// Linear regression coefficients (calculated from calibration points)
static float cal_slope = 0.0f;      // mV per °C
static float cal_intercept = 0.0f;  // mV at 0°C
static bool cal_valid = false;

// Manual override tracking
static time_t manual_override_until = 0;  // Timestamp when manual override expires (0 = not active)

// Calculate linear regression from calibration points
static void calculate_calibration_curve(void)
{
    if (cal_point_count < 2) {
        // Not enough points, use defaults
        cal_slope = (float)(DEFAULT_TEMP_30C_MV - DEFAULT_TEMP_20C_MV) / (30.0f - 20.0f);
        cal_intercept = DEFAULT_TEMP_20C_MV - cal_slope * 20.0f;
        cal_valid = false;
        ESP_LOGW(GPIO_TAG, "Using default calibration (not enough points)");
        return;
    }
    
    // Linear regression: voltage = slope * temp + intercept
    // Using least squares method
    float sum_temp = 0, sum_mv = 0, sum_temp_mv = 0, sum_temp_sq = 0;
    
    for (int i = 0; i < cal_point_count; i++) {
        float temp = (float)cal_points[i].temp_celsius;
        float mv = (float)cal_points[i].voltage_mv;
        
        sum_temp += temp;
        sum_mv += mv;
        sum_temp_mv += temp * mv;
        sum_temp_sq += temp * temp;
    }
    
    float n = (float)cal_point_count;
    cal_slope = (n * sum_temp_mv - sum_temp * sum_mv) / (n * sum_temp_sq - sum_temp * sum_temp);
    cal_intercept = (sum_mv - cal_slope * sum_temp) / n;
    cal_valid = true;
    
    ESP_LOGI(GPIO_TAG, "Calibration curve calculated: voltage = %.2f * temp + %.2f (from %d points)",
             cal_slope, cal_intercept, cal_point_count);
}

// Load temperature calibration from NVS
static void load_temp_calibration(void)
{
    nvs_handle_t nvs_handle;
    esp_err_t ret = nvs_open(NVS_TEMP_CAL_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGI(GPIO_TAG, "No stored temperature calibration, using defaults");
        cal_point_count = 0;
        calculate_calibration_curve();
        return;
    }
    
    // Load calibration point count
    uint8_t count = 0;
    ret = nvs_get_u8(nvs_handle, NVS_CAL_COUNT_KEY, &count);
    if (ret != ESP_OK || count == 0 || count > MAX_CALIBRATION_POINTS) {
        ESP_LOGI(GPIO_TAG, "Invalid calibration count, using defaults");
        nvs_close(nvs_handle);
        cal_point_count = 0;
        calculate_calibration_curve();
        return;
    }
    
    // Load calibration points
    cal_point_count = 0;
    for (int i = 0; i < count; i++) {
        char temp_key[16], mv_key[16];
        snprintf(temp_key, sizeof(temp_key), "%s%d", NVS_CAL_TEMP_PREFIX, i);
        snprintf(mv_key, sizeof(mv_key), "%s%d", NVS_CAL_MV_PREFIX, i);
        
        int16_t temp;
        uint16_t mv;
        if (nvs_get_i16(nvs_handle, temp_key, &temp) == ESP_OK &&
            nvs_get_u16(nvs_handle, mv_key, &mv) == ESP_OK) {
            cal_points[cal_point_count].temp_celsius = temp;
            cal_points[cal_point_count].voltage_mv = mv;
            cal_point_count++;
        }
    }
    
    nvs_close(nvs_handle);
    
    ESP_LOGI(GPIO_TAG, "Loaded %d calibration points from NVS", cal_point_count);
    calculate_calibration_curve();
}

// Save temperature calibration to NVS
static esp_err_t save_temp_calibration(void)
{
    nvs_handle_t nvs_handle;
    esp_err_t ret = nvs_open(NVS_TEMP_CAL_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(GPIO_TAG, "Failed to open NVS for temperature calibration");
        return ret;
    }
    
    // Save calibration point count
    nvs_set_u8(nvs_handle, NVS_CAL_COUNT_KEY, cal_point_count);
    
    // Save calibration points
    for (int i = 0; i < cal_point_count; i++) {
        char temp_key[16], mv_key[16];
        snprintf(temp_key, sizeof(temp_key), "%s%d", NVS_CAL_TEMP_PREFIX, i);
        snprintf(mv_key, sizeof(mv_key), "%s%d", NVS_CAL_MV_PREFIX, i);
        
        nvs_set_i16(nvs_handle, temp_key, cal_points[i].temp_celsius);
        nvs_set_u16(nvs_handle, mv_key, cal_points[i].voltage_mv);
    }
    
    ret = nvs_commit(nvs_handle);
    nvs_close(nvs_handle);
    
    ESP_LOGI(GPIO_TAG, "Saved %d calibration points to NVS", cal_point_count);
    
    return ret;
}

esp_err_t gpio_manager_init(void)
{
    esp_err_t ret;

    // Initialize all 4 GPIOs for H-bridge relay control
    gpio_config_t relay_io_conf = {
        .pin_bit_mask = (1ULL << RELAY_GPIO_1A) | (1ULL << RELAY_GPIO_1B) | 
                        (1ULL << RELAY_GPIO_2A) | (1ULL << RELAY_GPIO_2B),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ret = gpio_config(&relay_io_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(GPIO_TAG, "GPIO config failed: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Restore relay state from NVS
    nvs_handle_t nvs_handle;
    ret = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (ret == ESP_OK) {
        uint8_t saved_state = 0;
        ret = nvs_get_u8(nvs_handle, NVS_RELAY_KEY, &saved_state);
        if (ret == ESP_OK) {
            relay_state = saved_state;
            ESP_LOGI(GPIO_TAG, "Restored relay state from NVS: %d", relay_state);
        } else {
            relay_state = 0;
            ESP_LOGI(GPIO_TAG, "No saved relay state, defaulting to OFF");
        }
        
        // Restore manual override timestamp from NVS
        int64_t saved_override = 0;
        ret = nvs_get_i64(nvs_handle, NVS_MANUAL_OVERRIDE_KEY, &saved_override);
        if (ret == ESP_OK && saved_override > 0) {
            manual_override_until = (time_t)saved_override;
            
            // Check if override is still valid
            time_t now;
            time(&now);
            if (now < manual_override_until) {
                ESP_LOGI(GPIO_TAG, "Restored manual override from NVS (expires at %ld)", (long)manual_override_until);
            } else {
                ESP_LOGI(GPIO_TAG, "Manual override expired during sleep, clearing");
                manual_override_until = 0;
                nvs_set_i64(nvs_handle, NVS_MANUAL_OVERRIDE_KEY, 0);
                nvs_commit(nvs_handle);
            }
        } else {
            manual_override_until = 0;
        }
        
        nvs_close(nvs_handle);
    } else {
        relay_state = 0;
        manual_override_until = 0;
        ESP_LOGW(GPIO_TAG, "Failed to open NVS, defaulting relay to OFF");
    }
    
    // Set GPIOs to OFF state (all LOW for bistable relay)
    // Bistable relay holds its state mechanically, GPIOs only needed during switching
    gpio_set_level(RELAY_GPIO_1A, 0);
    gpio_set_level(RELAY_GPIO_1B, 0);
    gpio_set_level(RELAY_GPIO_2A, 0);
    gpio_set_level(RELAY_GPIO_2B, 0);
    
    // Configure all GPIOs to hold their state during deep sleep
    // TEMPORARILY DISABLED FOR TESTING
    // gpio_hold_en(RELAY_GPIO_1A);
    // gpio_hold_en(RELAY_GPIO_1B);
    // gpio_hold_en(RELAY_GPIO_2A);
    // gpio_hold_en(RELAY_GPIO_2B);
    // gpio_deep_sleep_hold_en();
    
    ESP_LOGI(GPIO_TAG, "Bistable relay initialized: All GPIOs=LOW, relay state=%s (from NVS)", 
             relay_state ? "ON" : "OFF");

    // Initialize GPIO3 for temperature ADC without pull-up
    gpio_config_t adc_io_conf = {
        .pin_bit_mask = (1ULL << ADC_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ret = gpio_config(&adc_io_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(GPIO_TAG, "GPIO%d config failed: %s", ADC_GPIO, esp_err_to_name(ret));
        return ret;
    }
    
    // Initialize GPIO2 for battery voltage ADC without pull-up
    gpio_config_t battery_io_conf = {
        .pin_bit_mask = (1ULL << BATTERY_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ret = gpio_config(&battery_io_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(GPIO_TAG, "GPIO%d config failed: %s", BATTERY_GPIO, esp_err_to_name(ret));
        return ret;
    }
    
    // Configure ADC
    adc_oneshot_unit_init_cfg_t adc_init_config = {
        .unit_id = ADC_UNIT,
    };
    ret = adc_oneshot_new_unit(&adc_init_config, &adc_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(GPIO_TAG, "ADC init failed: %s", esp_err_to_name(ret));
        return ret;
    }
    
    adc_oneshot_chan_cfg_t adc_chan_config = {
        .atten = ADC_ATTEN,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    ret = adc_oneshot_config_channel(adc_handle, ADC_CHANNEL, &adc_chan_config);
    if (ret != ESP_OK) {
        ESP_LOGE(GPIO_TAG, "ADC channel config failed: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Configure battery voltage ADC channel (GPIO2)
    ret = adc_oneshot_config_channel(adc_handle, BATTERY_ADC_CHANNEL, &adc_chan_config);
    if (ret != ESP_OK) {
        ESP_LOGE(GPIO_TAG, "Battery ADC channel config failed: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Initialize ADC calibration (curve fitting for ESP32-C3)
    adc_cali_curve_fitting_config_t cali_config = {
        .unit_id = ADC_UNIT,
        .atten = ADC_ATTEN,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    ret = adc_cali_create_scheme_curve_fitting(&cali_config, &adc_cali_handle);
    if (ret == ESP_OK) {
        adc_calibrated = true;
        ESP_LOGI(GPIO_TAG, "ADC calibration successful");
    } else {
        ESP_LOGW(GPIO_TAG, "ADC calibration failed, using raw values: %s", esp_err_to_name(ret));
        adc_calibrated = false;
    }
    
    ESP_LOGI(GPIO_TAG, "GPIO%d initialized for temperature ADC input", ADC_GPIO);
    ESP_LOGI(GPIO_TAG, "GPIO%d initialized for battery voltage ADC input", BATTERY_GPIO);

    // Load temperature calibration from NVS
    load_temp_calibration();

    return ESP_OK;
}

void gpio_set_relay(uint8_t state)
{
    relay_state = state ? 1 : 0;
    
    // Disable hold to allow GPIO changes
    gpio_hold_dis(RELAY_GPIO_1A);
    gpio_hold_dis(RELAY_GPIO_1B);
    gpio_hold_dis(RELAY_GPIO_2A);
    gpio_hold_dis(RELAY_GPIO_2B);
    
    // Ensure GPIOs are configured as outputs
    gpio_set_direction(RELAY_GPIO_1A, GPIO_MODE_OUTPUT);
    gpio_set_direction(RELAY_GPIO_1B, GPIO_MODE_OUTPUT);
    gpio_set_direction(RELAY_GPIO_2A, GPIO_MODE_OUTPUT);
    gpio_set_direction(RELAY_GPIO_2B, GPIO_MODE_OUTPUT);
    
    // Bistable relay control: Apply pulse for 1 second, then turn off
    if (relay_state == 1) {
        // Turn relay ON: GPIO6/7 = HIGH, GPIO4/5 = LOW
        ESP_LOGI(GPIO_TAG, "Switching bistable relay ON (GPIO6/7=HIGH, GPIO4/5=LOW for 1s)");
        gpio_set_level(RELAY_GPIO_1A, 1);
        gpio_set_level(RELAY_GPIO_1B, 1);
        gpio_set_level(RELAY_GPIO_2A, 0);
        gpio_set_level(RELAY_GPIO_2B, 0);
    } else {
        // Turn relay OFF: GPIO6/7 = LOW, GPIO4/5 = HIGH
        ESP_LOGI(GPIO_TAG, "Switching bistable relay OFF (GPIO6/7=LOW, GPIO4/5=HIGH for 1s)");
        gpio_set_level(RELAY_GPIO_1A, 0);
        gpio_set_level(RELAY_GPIO_1B, 0);
        gpio_set_level(RELAY_GPIO_2A, 1);
        gpio_set_level(RELAY_GPIO_2B, 1);
    }
    
    // Wait for relay to switch
    ESP_LOGI(GPIO_TAG, "Holding GPIO state for %d milliseconds...", RELAY_PULSE_DURATION_MS);
    vTaskDelay(pdMS_TO_TICKS(RELAY_PULSE_DURATION_MS));
    
    // Turn off all GPIOs (bistable relay holds state)
    ESP_LOGI(GPIO_TAG, "Bistable relay switched, turning off all GPIOs");
    gpio_set_level(RELAY_GPIO_1A, 0);
    gpio_set_level(RELAY_GPIO_1B, 0);
    gpio_set_level(RELAY_GPIO_2A, 0);
    gpio_set_level(RELAY_GPIO_2B, 0);
    
    // Re-enable hold for deep sleep (all GPIOs at 0)
    gpio_hold_en(RELAY_GPIO_1A);
    gpio_hold_en(RELAY_GPIO_1B);
    gpio_hold_en(RELAY_GPIO_2A);
    gpio_hold_en(RELAY_GPIO_2B);
    gpio_deep_sleep_hold_en();
    
    // Save state to NVS
    nvs_handle_t nvs_handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (ret == ESP_OK) {
        nvs_set_u8(nvs_handle, NVS_RELAY_KEY, relay_state);
        nvs_commit(nvs_handle);
        nvs_close(nvs_handle);
    }
    
    ESP_LOGI(GPIO_TAG, "Bistable relay state: %s (saved to NVS)", relay_state ? "ON" : "OFF");
}

uint8_t gpio_get_relay(void)
{
    return relay_state;
}

uint16_t gpio_read_adc_voltage(void)
{
    int adc_raw = 0;
    esp_err_t ret = adc_oneshot_read(adc_handle, ADC_CHANNEL, &adc_raw);
    if (ret == ESP_OK) {
        if (adc_calibrated) {
            int voltage_mv = 0;
            adc_cali_raw_to_voltage(adc_cali_handle, adc_raw, &voltage_mv);
            return (uint16_t)voltage_mv;
        } else {
            // Approximate conversion if not calibrated (for 12-bit ADC, 0-3.3V)
            return (uint16_t)((adc_raw * 3300) / 4095);
        }
    }
    ESP_LOGE(GPIO_TAG, "ADC read failed: %s", esp_err_to_name(ret));
    return 0;
}

int16_t gpio_read_temperature(void)
{
    uint16_t voltage_mv = gpio_read_adc_voltage();
    
    if (voltage_mv == 0) {
        return 0;  // Error reading ADC
    }
    
    // Calculate temperature using linear regression: temp = (voltage - intercept) / slope
    float temp_float = (voltage_mv - cal_intercept) / cal_slope;
    int16_t temp = (int16_t)(temp_float + 0.5f);  // Round to nearest integer
    
    ESP_LOGI(GPIO_TAG, "ADC: %d mV -> Temperature: %d°C (using %s calibration)",
             voltage_mv, temp, cal_valid ? "custom" : "default");
    
    return temp;
}

void gpio_set_relay_manual(uint8_t state)
{
    // Set manual override timestamp (1 hour from now)
    time_t now;
    time(&now);
    manual_override_until = now + MANUAL_OVERRIDE_DURATION_SEC;
    
    ESP_LOGI(GPIO_TAG, "Manual override activated for 1 hour (until %ld)", (long)manual_override_until);
    
    // Save manual override timestamp to NVS
    nvs_handle_t nvs_handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (ret == ESP_OK) {
        nvs_set_i64(nvs_handle, NVS_MANUAL_OVERRIDE_KEY, (int64_t)manual_override_until);
        nvs_commit(nvs_handle);
        nvs_close(nvs_handle);
        ESP_LOGI(GPIO_TAG, "Manual override timestamp saved to NVS");
    }
    
    // Set the relay state
    gpio_set_relay(state);
}

bool gpio_is_manual_override_active(void)
{
    if (manual_override_until == 0) {
        return false;  // No override set
    }
    
    time_t now;
    time(&now);
    
    if (now >= manual_override_until) {
        // Override expired
        manual_override_until = 0;
        ESP_LOGI(GPIO_TAG, "Manual override expired, scheduler resumed");
        return false;
    }
    
    return true;  // Override still active
}

void gpio_clear_manual_override(void)
{
    if (manual_override_until != 0) {
        ESP_LOGI(GPIO_TAG, "Manual override cleared");
        manual_override_until = 0;
        
        // Clear manual override timestamp from NVS
        nvs_handle_t nvs_handle;
        esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
        if (ret == ESP_OK) {
            nvs_set_i64(nvs_handle, NVS_MANUAL_OVERRIDE_KEY, 0);
            nvs_commit(nvs_handle);
            nvs_close(nvs_handle);
        }
    }
}

esp_err_t gpio_calibrate_temperature(int16_t actual_temp_celsius)
{
    // Read current voltage
    uint16_t current_mv = gpio_read_adc_voltage();
    
    if (current_mv == 0) {
        ESP_LOGE(GPIO_TAG, "Failed to read ADC for calibration");
        return ESP_FAIL;
    }
    
    // Check if we already have a point very close to this temperature (within 1°C)
    for (int i = 0; i < cal_point_count; i++) {
        if (abs(cal_points[i].temp_celsius - actual_temp_celsius) <= 1) {
            // Update existing point
            ESP_LOGI(GPIO_TAG, "Updating existing calibration point: %d°C@%dmV -> %d°C@%dmV",
                     cal_points[i].temp_celsius, cal_points[i].voltage_mv,
                     actual_temp_celsius, current_mv);
            cal_points[i].temp_celsius = actual_temp_celsius;
            cal_points[i].voltage_mv = current_mv;
            calculate_calibration_curve();
            return save_temp_calibration();
        }
    }
    
    // Add new calibration point
    if (cal_point_count < MAX_CALIBRATION_POINTS) {
        cal_points[cal_point_count].temp_celsius = actual_temp_celsius;
        cal_points[cal_point_count].voltage_mv = current_mv;
        cal_point_count++;
        ESP_LOGI(GPIO_TAG, "Added calibration point %d: %d°C@%dmV",
                 cal_point_count, actual_temp_celsius, current_mv);
    } else {
        // Replace oldest point (FIFO)
        ESP_LOGI(GPIO_TAG, "Calibration buffer full, replacing oldest point");
        for (int i = 0; i < MAX_CALIBRATION_POINTS - 1; i++) {
            cal_points[i] = cal_points[i + 1];
        }
        cal_points[MAX_CALIBRATION_POINTS - 1].temp_celsius = actual_temp_celsius;
        cal_points[MAX_CALIBRATION_POINTS - 1].voltage_mv = current_mv;
        ESP_LOGI(GPIO_TAG, "Added calibration point: %d°C@%dmV", actual_temp_celsius, current_mv);
    }
    
    calculate_calibration_curve();
    return save_temp_calibration();
}

esp_err_t gpio_reset_temperature_calibration(void)
{
    ESP_LOGI(GPIO_TAG, "Resetting temperature calibration to factory defaults");
    
    // Clear all calibration points
    cal_point_count = 0;
    
    // Recalculate using defaults
    calculate_calibration_curve();
    
    // Clear NVS
    nvs_handle_t nvs_handle;
    esp_err_t ret = nvs_open(NVS_TEMP_CAL_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (ret == ESP_OK) {
        nvs_erase_all(nvs_handle);
        nvs_commit(nvs_handle);
        nvs_close(nvs_handle);
    }
    
    ESP_LOGI(GPIO_TAG, "Temperature calibration reset complete");
    return ESP_OK;
}

uint16_t gpio_read_battery_voltage(void)
{
    int adc_raw = 0;
    esp_err_t ret = adc_oneshot_read(adc_handle, BATTERY_ADC_CHANNEL, &adc_raw);
    if (ret == ESP_OK) {
        uint16_t voltage_mv = 0;
        if (adc_calibrated) {
            int voltage_mv_int = 0;
            adc_cali_raw_to_voltage(adc_cali_handle, adc_raw, &voltage_mv_int);
            voltage_mv = (uint16_t)voltage_mv_int;
        } else {
            // Approximate conversion if not calibrated (for 12-bit ADC, 0-3.3V)
            voltage_mv = (uint16_t)((adc_raw * 3300) / 4095);
        }
        
        // Multiply by 2 to get actual battery voltage (voltage divider)
        uint16_t battery_voltage = voltage_mv * 2;
        
        ESP_LOGI(GPIO_TAG, "Battery ADC: raw=%d, measured=%d mV, actual=%d mV", 
                 adc_raw, voltage_mv, battery_voltage);
        
        return battery_voltage;
    }
    ESP_LOGE(GPIO_TAG, "Battery ADC read failed: %s", esp_err_to_name(ret));
    return 0;
}

uint8_t gpio_get_battery_percentage(void)
{
    uint16_t voltage_mv = gpio_read_battery_voltage();
    
    // Battery voltage to percentage conversion (from config.h)
    if (voltage_mv >= BATTERY_MAX_MV) {
        return 100;
    } else if (voltage_mv <= BATTERY_MIN_MV) {
        return 0;
    }
    
    // Linear interpolation
    uint32_t percentage = ((uint32_t)(voltage_mv - BATTERY_MIN_MV) * 100) / (BATTERY_MAX_MV - BATTERY_MIN_MV);
    return (uint8_t)percentage;
}
