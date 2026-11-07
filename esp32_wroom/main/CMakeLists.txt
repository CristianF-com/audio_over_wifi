# El código es el MISMO que el del XIAO ESP32-C5 (../../esp32/main): así una mejora en uno vale para los dos.
# Esta carpeta solo cambia el chip (esp32), los pines por defecto y la configuración (sdkconfig.defaults / Kconfig).
idf_component_register(
    SRCS "../../esp32/main/receiver_main.c" "../../esp32/main/ble_prov.c" "../../esp32/main/ota_update.c"
    INCLUDE_DIRS "." "../../esp32/main" "../../common"      # protocol.h compartido con la RPi
    REQUIRES
        esp_wifi
        esp_event
        esp_netif
        esp_timer
        esp_driver_i2s
        nvs_flash
        lwip
        bt
        app_update
        esp_http_client
        esp_https_ota
)
