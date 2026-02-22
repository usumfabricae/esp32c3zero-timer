#ifndef BLE_SERVER_H
#define BLE_SERVER_H

#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

esp_err_t ble_server_init(void);
void ble_server_deinit(void);
bool ble_is_connected(void);
void ble_update_standard_time(void);
void ble_update_standard_temperature(void);
void ble_update_battery_level(void);
uint8_t ble_get_relay_state(void);
uint16_t ble_get_adc_voltage(void);
int16_t ble_get_temperature(void);

#endif // BLE_SERVER_H
