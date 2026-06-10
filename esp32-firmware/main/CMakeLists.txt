idf_component_register(
    SRCS "main.c" "wifi_manager.c"
    INCLUDE_DIRS "."
    REQUIRES nvs_flash esp_wifi esp_event driver esp_adc json esp_http_client esp-tls lwip esp_timer esp_http_server
)
