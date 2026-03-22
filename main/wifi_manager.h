#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include "esp_err.h"
#include <stdbool.h>

esp_err_t wifi_init_sta(void);
void wifi_wait_connected(void);
esp_err_t sync_time_with_ntp(void);

// IoT Core integration functions
esp_err_t wifi_manager_connect_with_timeout(uint32_t timeout_sec);
esp_err_t wifi_manager_disconnect_immediate(void);
void wifi_manager_check_signal_strength(void);
bool wifi_manager_is_connected(void);

#endif // WIFI_MANAGER_H
