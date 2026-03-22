#include "cert_store.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "cert_store";

// NVS namespace for IoT Core credentials
#define NVS_NAMESPACE "iot_creds"

// NVS keys
#define NVS_KEY_DEVICE_CERT "iot_cert"
#define NVS_KEY_PRIVATE_KEY "iot_key"
#define NVS_KEY_ROOT_CA "iot_ca"

// Maximum certificate sizes (PEM format)
#define MAX_CERT_SIZE 2048
#define MAX_KEY_SIZE 2048
#define MAX_CA_SIZE 2048

esp_err_t cert_store_init(void)
{
    // NVS is already initialized in main.c
    // Just verify we can open the namespace
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err == ESP_OK) {
        nvs_close(handle);
        ESP_LOGI(TAG, "Certificate store initialized");
        return ESP_OK;
    } else if (err == ESP_ERR_NVS_NOT_FOUND) {
        // Namespace doesn't exist yet, will be created on first write
        ESP_LOGI(TAG, "Certificate store namespace not found (no credentials provisioned)");
        return ESP_OK;
    } else {
        ESP_LOGE(TAG, "Failed to open NVS namespace: %s", esp_err_to_name(err));
        return ESP_FAIL;
    }
}

bool cert_store_has_credentials(void)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        return false;
    }

    // Check if all three credentials exist
    size_t required_size;
    bool has_cert = (nvs_get_str(handle, NVS_KEY_DEVICE_CERT, NULL, &required_size) == ESP_OK);
    bool has_key = (nvs_get_str(handle, NVS_KEY_PRIVATE_KEY, NULL, &required_size) == ESP_OK);
    bool has_ca = (nvs_get_str(handle, NVS_KEY_ROOT_CA, NULL, &required_size) == ESP_OK);

    nvs_close(handle);

    bool has_all = has_cert && has_key && has_ca;
    ESP_LOGI(TAG, "Credentials check: cert=%d, key=%d, ca=%d", has_cert, has_key, has_ca);
    return has_all;
}

esp_err_t cert_store_get_device_cert(char *buf, size_t buf_len)
{
    if (buf == NULL || buf_len == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS: %s", esp_err_to_name(err));
        return ESP_FAIL;
    }

    size_t required_size = buf_len;
    err = nvs_get_str(handle, NVS_KEY_DEVICE_CERT, buf, &required_size);
    nvs_close(handle);

    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Device certificate retrieved (%d bytes)", required_size);
    } else if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "Device certificate not found");
    } else if (err == ESP_ERR_NVS_INVALID_LENGTH) {
        ESP_LOGE(TAG, "Buffer too small for device certificate (need %d bytes)", required_size);
        return ESP_ERR_INVALID_SIZE;
    } else {
        ESP_LOGE(TAG, "Failed to get device certificate: %s", esp_err_to_name(err));
    }

    return err;
}

esp_err_t cert_store_get_private_key(char *buf, size_t buf_len)
{
    if (buf == NULL || buf_len == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS: %s", esp_err_to_name(err));
        return ESP_FAIL;
    }

    size_t required_size = buf_len;
    err = nvs_get_str(handle, NVS_KEY_PRIVATE_KEY, buf, &required_size);
    nvs_close(handle);

    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Private key retrieved (%d bytes)", required_size);
    } else if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "Private key not found");
    } else if (err == ESP_ERR_NVS_INVALID_LENGTH) {
        ESP_LOGE(TAG, "Buffer too small for private key (need %d bytes)", required_size);
        return ESP_ERR_INVALID_SIZE;
    } else {
        ESP_LOGE(TAG, "Failed to get private key: %s", esp_err_to_name(err));
    }

    return err;
}

esp_err_t cert_store_get_root_ca(char *buf, size_t buf_len)
{
    if (buf == NULL || buf_len == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS: %s", esp_err_to_name(err));
        return ESP_FAIL;
    }

    size_t required_size = buf_len;
    err = nvs_get_str(handle, NVS_KEY_ROOT_CA, buf, &required_size);
    nvs_close(handle);

    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Root CA retrieved (%d bytes)", required_size);
    } else if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "Root CA not found");
    } else if (err == ESP_ERR_NVS_INVALID_LENGTH) {
        ESP_LOGE(TAG, "Buffer too small for root CA (need %d bytes)", required_size);
        return ESP_ERR_INVALID_SIZE;
    } else {
        ESP_LOGE(TAG, "Failed to get root CA: %s", esp_err_to_name(err));
    }

    return err;
}

esp_err_t cert_store_set_credentials(
    const char *device_cert,
    const char *private_key,
    const char *root_ca)
{
    if (device_cert == NULL || private_key == NULL || root_ca == NULL) {
        ESP_LOGE(TAG, "Invalid arguments: credentials cannot be NULL");
        return ESP_ERR_INVALID_ARG;
    }

    // Validate certificate sizes
    size_t cert_len = strlen(device_cert) + 1;  // +1 for null terminator
    size_t key_len = strlen(private_key) + 1;
    size_t ca_len = strlen(root_ca) + 1;

    if (cert_len > MAX_CERT_SIZE || key_len > MAX_KEY_SIZE || ca_len > MAX_CA_SIZE) {
        ESP_LOGE(TAG, "Certificate too large: cert=%d, key=%d, ca=%d", cert_len, key_len, ca_len);
        return ESP_ERR_INVALID_SIZE;
    }

    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS for writing: %s", esp_err_to_name(err));
        return ESP_FAIL;
    }

    // Store device certificate
    err = nvs_set_str(handle, NVS_KEY_DEVICE_CERT, device_cert);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to store device certificate: %s", esp_err_to_name(err));
        nvs_close(handle);
        return ESP_FAIL;
    }

    // Store private key
    err = nvs_set_str(handle, NVS_KEY_PRIVATE_KEY, private_key);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to store private key: %s", esp_err_to_name(err));
        nvs_close(handle);
        return ESP_FAIL;
    }

    // Store root CA
    err = nvs_set_str(handle, NVS_KEY_ROOT_CA, root_ca);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to store root CA: %s", esp_err_to_name(err));
        nvs_close(handle);
        return ESP_FAIL;
    }

    // Commit changes
    err = nvs_commit(handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to commit credentials: %s", esp_err_to_name(err));
        nvs_close(handle);
        return ESP_FAIL;
    }

    nvs_close(handle);
    ESP_LOGI(TAG, "Credentials stored successfully");
    return ESP_OK;
}
