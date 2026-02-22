#ifndef LED_STATUS_H
#define LED_STATUS_H

#include "esp_err.h"

typedef enum {
    LED_STATUS_OFF,             // Off - deep sleep
    LED_STATUS_SYNCING,         // Red - waiting for NTP sync
    LED_STATUS_SYNCED,          // Green - time synced
    LED_STATUS_BLE_CONNECTED,   // Blue - BLE client connected
    LED_STATUS_LOW_BATTERY      // Yellow - battery under 30%
} led_status_t;

esp_err_t led_status_init(void);
void led_status_set(led_status_t status);

#endif // LED_STATUS_H
