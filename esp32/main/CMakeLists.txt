idf_component_register(
    SRCS "receiver_main.c" "ble_prov.c" "ota_update.c"
    INCLUDE_DIRS "." "../../common"      # protocol.h compartido con la RPi
    REQUIRES
        esp_wifi
        esp_event
        esp_netif
        esp_timer
        esp_driver_i2s
        esp_driver_gpio
        nvs_flash
        lwip
        bt
        app_update
        esp_http_client
        esp_https_ota
)
