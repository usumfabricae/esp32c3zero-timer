#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>
#include <time.h>

// Schedule configuration
#define HOURS_PER_DAY 24
#define DAYS_PER_WEEK 7

// Schedule modes
#define SCHEDULE_MODE_HIGH 'H'
#define SCHEDULE_MODE_LOW  'L'
#define SCHEDULE_MODE_OFF  'O'  // Off mode (no control)

// Day of week (0=Monday, 6=Sunday)
typedef enum {
    DAY_MONDAY = 0,
    DAY_TUESDAY = 1,
    DAY_WEDNESDAY = 2,
    DAY_THURSDAY = 3,
    DAY_FRIDAY = 4,
    DAY_SATURDAY = 5,
    DAY_SUNDAY = 6
} day_of_week_t;

// Schedule structure
typedef struct {
    char schedule[DAYS_PER_WEEK][HOURS_PER_DAY + 1];  // +1 for null terminator
    int16_t high_temp;  // High temperature threshold in °C
    int16_t low_temp;   // Low temperature threshold in °C
} scheduler_config_t;

// Initialize scheduler (loads from NVS)
esp_err_t scheduler_init(void);

// Get current schedule configuration
esp_err_t scheduler_get_config(scheduler_config_t *config);

// Set schedule for a specific day (0=Monday, 6=Sunday)
esp_err_t scheduler_set_day_schedule(uint8_t day, const char *schedule);

// Set temperature thresholds
esp_err_t scheduler_set_temperatures(int16_t high_temp, int16_t low_temp);

// Get schedule mode for current time
char scheduler_get_current_mode(void);

// Check if relay should be on based on current schedule and temperature
bool scheduler_should_relay_be_on(int16_t current_temp);

// Save configuration to NVS
esp_err_t scheduler_save_config(void);

// Get day of week from time (0=Monday, 6=Sunday)
day_of_week_t scheduler_get_day_of_week(struct tm *timeinfo);

#endif // SCHEDULER_H
