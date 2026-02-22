#include "led_status.h"
#include "config.h"
#include "esp_log.h"
#include "driver/rmt_tx.h"
#include "led_strip_encoder.h"

#define LED_TAG "led_status"
#define LED_GPIO STATUS_LED_GPIO
#define LED_STRIP_RMT_RES_HZ (10 * 1000 * 1000)  // 10MHz resolution

static rmt_channel_handle_t led_chan = NULL;
static rmt_encoder_handle_t led_encoder = NULL;
static led_status_t current_status = LED_STATUS_SYNCING;

// RGB color values (GRB format for WS2812) - brightness from config.h
static const uint8_t color_red[3] = {0, LED_BRIGHTNESS, 0};      // GRB: Green=0, Red=brightness, Blue=0
static const uint8_t color_green[3] = {LED_BRIGHTNESS, 0, 0};    // GRB: Green=brightness, Red=0, Blue=0
static const uint8_t color_blue[3] = {0, 0, LED_BRIGHTNESS};     // GRB: Green=0, Red=0, Blue=brightness
static const uint8_t color_yellow[3] = {LED_BRIGHTNESS, LED_BRIGHTNESS, 0};  // GRB: Green=brightness, Red=brightness, Blue=0

esp_err_t led_status_init(void)
{
    ESP_LOGI(LED_TAG, "Initializing WS2812 LED on GPIO %d", LED_GPIO);
    
    // RMT TX channel configuration
    rmt_tx_channel_config_t tx_chan_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .gpio_num = LED_GPIO,
        .mem_block_symbols = 64,
        .resolution_hz = LED_STRIP_RMT_RES_HZ,
        .trans_queue_depth = 4,
    };
    
    esp_err_t ret = rmt_new_tx_channel(&tx_chan_config, &led_chan);
    if (ret != ESP_OK) {
        ESP_LOGE(LED_TAG, "Failed to create RMT TX channel: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // LED strip encoder configuration
    led_strip_encoder_config_t encoder_config = {
        .resolution = LED_STRIP_RMT_RES_HZ,
    };
    
    ret = rmt_new_led_strip_encoder(&encoder_config, &led_encoder);
    if (ret != ESP_OK) {
        ESP_LOGE(LED_TAG, "Failed to create LED strip encoder: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ret = rmt_enable(led_chan);
    if (ret != ESP_OK) {
        ESP_LOGE(LED_TAG, "Failed to enable RMT channel: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Set initial status (red - syncing)
    led_status_set(LED_STATUS_SYNCING);
    
    ESP_LOGI(LED_TAG, "LED initialized successfully");
    return ESP_OK;
}

void led_status_set(led_status_t status)
{
    if (led_chan == NULL || led_encoder == NULL) {
        ESP_LOGW(LED_TAG, "LED not initialized");
        return;
    }
    
    current_status = status;
    const uint8_t *color;
    const char *status_name;
    static const uint8_t color_off[3] = {0, 0, 0};  // All off
    
    switch (status) {
        case LED_STATUS_OFF:
            color = color_off;
            status_name = "OFF";
            break;
        case LED_STATUS_SYNCING:
            color = color_red;
            status_name = "SYNCING (Red)";
            break;
        case LED_STATUS_SYNCED:
            color = color_green;
            status_name = "SYNCED (Green)";
            break;
        case LED_STATUS_BLE_CONNECTED:
            color = color_blue;
            status_name = "BLE_CONNECTED (Blue)";
            break;
        case LED_STATUS_LOW_BATTERY:
            color = color_yellow;
            status_name = "LOW_BATTERY (Yellow)";
            break;
        default:
            ESP_LOGW(LED_TAG, "Unknown status: %d", status);
            return;
    }
    
    rmt_transmit_config_t tx_config = {
        .loop_count = 0,
    };
    
    esp_err_t ret = rmt_transmit(led_chan, led_encoder, color, 3, &tx_config);
    if (ret != ESP_OK) {
        ESP_LOGE(LED_TAG, "Failed to transmit LED data: %s", esp_err_to_name(ret));
    } else {
        ESP_LOGI(LED_TAG, "LED status set to: %s", status_name);
    }
    
    // Wait for transmission to complete
    rmt_tx_wait_all_done(led_chan, 100);
}
