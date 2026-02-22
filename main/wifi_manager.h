#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include "esp_err.h"

esp_err_t wifi_init_sta(void);
void wifi_wait_connected(void);
esp_err_t sync_time_with_ntp(void);

#endif // WIFI_MANAGER_H
