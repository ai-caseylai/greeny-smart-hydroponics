idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES nvs_flash esp_wifi esp_event driver esp_adc json esp_http_client esp-tls lwip
)
