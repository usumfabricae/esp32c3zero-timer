idf_component_register(SRCS "main.c" "wifi_manager.c" "ble_server.c" "led_status.c" "led_strip_encoder.c" "gpio_manager.c" "scheduler.c"
                    INCLUDE_DIRS "."
                    REQUIRES bt nvs_flash esp_wifi driver esp_adc)
