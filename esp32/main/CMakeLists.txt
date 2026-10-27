idf_component_register(
    SRCS "receiver_main.c" "ble_prov.c"
    INCLUDE_DIRS "." "../../common"      # protocol.h compartido con la RPi
    REQUIRES
        esp_wifi
        esp_event
        esp_netif
        esp_timer
        esp_driver_i2s
        nvs_flash
        lwip
        bt
)
