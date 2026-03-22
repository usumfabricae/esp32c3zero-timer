#ifndef CERT_STORE_H
#define CERT_STORE_H

#include "esp_err.h"
#include <stdbool.h>
#include <stddef.h>

/**
 * @brief Initialize certificate store
 * 
 * Opens NVS namespace for certificate storage
 * 
 * @return ESP_OK on success, ESP_FAIL on error
 */
esp_err_t cert_store_init(void);

/**
 * @brief Check if IoT Core credentials are provisioned
 * 
 * @return true if all credentials (cert, key, CA) are present, false otherwise
 */
bool cert_store_has_credentials(void);

/**
 * @brief Get device certificate from NVS
 * 
 * @param buf Buffer to store certificate (PEM format)
 * @param buf_len Size of buffer
 * @return ESP_OK on success, ESP_ERR_NVS_NOT_FOUND if not found, ESP_ERR_INVALID_SIZE if buffer too small
 */
esp_err_t cert_store_get_device_cert(char *buf, size_t buf_len);

/**
 * @brief Get private key from NVS
 * 
 * @param buf Buffer to store private key (PEM format)
 * @param buf_len Size of buffer
 * @return ESP_OK on success, ESP_ERR_NVS_NOT_FOUND if not found, ESP_ERR_INVALID_SIZE if buffer too small
 */
esp_err_t cert_store_get_private_key(char *buf, size_t buf_len);

/**
 * @brief Get root CA certificate from NVS
 * 
 * @param buf Buffer to store root CA (PEM format)
 * @param buf_len Size of buffer
 * @return ESP_OK on success, ESP_ERR_NVS_NOT_FOUND if not found, ESP_ERR_INVALID_SIZE if buffer too small
 */
esp_err_t cert_store_get_root_ca(char *buf, size_t buf_len);

/**
 * @brief Store IoT Core credentials in NVS
 * 
 * Used during device provisioning to store certificates
 * 
 * @param device_cert Device certificate (PEM format, null-terminated)
 * @param private_key Private key (PEM format, null-terminated)
 * @param root_ca AWS root CA certificate (PEM format, null-terminated)
 * @return ESP_OK on success, ESP_FAIL on error
 */
esp_err_t cert_store_set_credentials(
    const char *device_cert,
    const char *private_key,
    const char *root_ca
);

#endif // CERT_STORE_H
