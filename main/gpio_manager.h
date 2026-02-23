#ifndef GPIO_MANAGER_H
#define GPIO_MANAGER_H

#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>
#include <time.h>

// Initialize all GPIO pins
esp_err_t gpio_manager_init(void);

// Bistable relay control (latching relay)
// Requires 1-second pulse to switch, then all GPIOs go to 0
// state=1 (ON): GPIO6/7=HIGH, GPIO4/5=LOW for 1s, then all=0
// state=0 (OFF): GPIO6/7=LOW, GPIO4/5=HIGH for 1s, then all=0
void gpio_set_relay(uint8_t state);
void gpio_set_relay_manual(uint8_t state, uint16_t duration_minutes);  // Manual override with custom duration
uint8_t gpio_get_relay(void);

// Manual override control
bool gpio_is_manual_override_active(void);  // Check if manual override is active
void gpio_clear_manual_override(void);      // Clear manual override (for testing)
time_t gpio_get_manual_override_endtime(void);  // Get override end time (0 if not active)

// Temperature calibration
esp_err_t gpio_calibrate_temperature(int16_t actual_temp_celsius);  // Add calibration point
esp_err_t gpio_reset_temperature_calibration(void);                 // Reset to factory defaults

// Temperature reading (GPIO3)
int16_t gpio_read_temperature(void);  // Returns temperature in degrees Celsius
uint16_t gpio_read_adc_voltage(void);  // Returns raw voltage in mV (for debugging)

// Battery voltage reading (GPIO2)
uint16_t gpio_read_battery_voltage(void);  // Returns battery voltage in mV (actual voltage, already multiplied by 2)
uint8_t gpio_get_battery_percentage(void);  // Returns battery percentage (0-100%)

#endif // GPIO_MANAGER_H
