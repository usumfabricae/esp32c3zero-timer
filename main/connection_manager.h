#ifndef CONNECTION_MANAGER_H
#define CONNECTION_MANAGER_H

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief Connection mode enumeration
 * 
 * Defines the two operating modes for the device:
 * - IoT Mode: Cloud connected via AWS IoT Core (5-minute wake cycles)
 * - BLE Mode: Local fallback via Bluetooth LE (1-minute wake cycles)
 */
typedef enum {
    CONNECTION_MODE_IOT,   // Cloud connected (5-min cycle, WiFi-based)
    CONNECTION_MODE_BLE    // Local fallback (1-min cycle, BLE-based)
} connection_mode_t;

/**
 * @brief Initialize connection manager
 * 
 * Reads IoT Core reachability state from RTC memory and NVS.
 * Must be called early in boot sequence before mode selection.
 */
void connection_manager_init(void);

/**
 * @brief Determine which connection mode to use this wake cycle
 * 
 * Returns IoT mode if IoT Core is marked as reachable, otherwise BLE mode.
 * Decision is based on reachability state persisted in RTC memory.
 * 
 * @return CONNECTION_MODE_IOT or CONNECTION_MODE_BLE
 */
connection_mode_t connection_manager_get_mode(void);

/**
 * @brief Calculate sleep duration based on current mode
 * 
 * Returns mode-specific sleep duration:
 * - IoT mode: 300 seconds (5 minutes)
 * - BLE mode: 54 seconds (wake at XX:XX:59 each minute)
 * 
 * @return Sleep duration in seconds
 */
uint32_t connection_manager_get_sleep_duration_sec(void);

/**
 * @brief Update connection mode based on sync result
 * 
 * Updates IoT Core reachability state based on sync session outcome:
 * - Success: Mark IoT Core as reachable, switch to IoT mode
 * - Failure: Mark IoT Core as unreachable, switch to BLE mode
 * 
 * State is persisted in both RTC memory (for deep sleep) and NVS (for power cycles).
 * 
 * @param iot_success true if IoT sync succeeded, false if failed
 */
void connection_manager_update_mode(bool iot_success);

/**
 * @brief Check if this wake cycle should retry IoT Core connection
 * 
 * When in BLE mode, device retries IoT Core connection once per hour.
 * This function tracks wake count to determine if hourly retry is due.
 * 
 * @return true if should retry IoT Core, false otherwise
 */
bool connection_manager_should_retry_iot(void);

#endif // CONNECTION_MANAGER_H
