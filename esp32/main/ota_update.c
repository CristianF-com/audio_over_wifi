/* Actualización de firmware por WiFi (OTA) — ver ota_update.h.
 * Requisitos de configuración (sdkconfig.defaults): tabla de particiones con ota_0/ota_1 (partitions.csv),
 * CONFIG_ESP_HTTPS_OTA_ALLOW_HTTP=y (la descarga va por HTTP, en la LAN) y CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE=y.
 * Seguridad: el token de un solo uso evita que cualquiera de la red pida el binario; NO hay firma del firmware. */
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_ota_ops.h"
#include "esp_http_client.h"
#include "esp_https_ota.h"
#include "ota_update.h"

static const char* TAG = "ota";
static volatile bool s_active = false;
static char     s_url[112];
static uint32_t s_size;

bool ota_update_active(void) { return s_active; }

static void ota_task(void* pv)
{
    (void)pv;
    esp_http_client_config_t http = { .url = s_url, .timeout_ms = 10000, .keep_alive_enable = true };
    esp_https_ota_config_t cfg = { .http_config = &http };
    esp_https_ota_handle_t h = NULL;
    esp_err_t e = esp_https_ota_begin(&cfg, &h);
    if (e != ESP_OK) { ESP_LOGE(TAG, "no se pudo empezar la descarga: %s", esp_err_to_name(e)); goto fail; }
    while ((e = esp_https_ota_perform(h)) == ESP_ERR_HTTPS_OTA_IN_PROGRESS) { /* descargando y grabando */ }
    if (e == ESP_OK && s_size && esp_https_ota_get_image_len_read(h) != (int)s_size) {
        ESP_LOGE(TAG, "tamaño recibido %d != anunciado %u", esp_https_ota_get_image_len_read(h), (unsigned)s_size);
        e = ESP_ERR_INVALID_SIZE;
    }
    if (e == ESP_OK && !esp_https_ota_is_complete_data_received(h)) e = ESP_FAIL;
    if (e != ESP_OK) { esp_https_ota_abort(h); ESP_LOGE(TAG, "descarga fallida: %s", esp_err_to_name(e)); goto fail; }
    e = esp_https_ota_finish(h);                    /* valida la imagen y activa la otra partición */
    if (e != ESP_OK) { ESP_LOGE(TAG, "imagen no válida: %s", esp_err_to_name(e)); goto fail; }
    ESP_LOGI(TAG, "actualización correcta: reiniciando");
    vTaskDelay(pdMS_TO_TICKS(500));
    esp_restart();
fail:
    s_active = false;                               /* vuelve a anunciarse (HELLO) y la RPi da por terminada la OTA */
    vTaskDelete(NULL);
}

void ota_update_start(const char* rpi_ip, uint16_t port, uint32_t size, uint64_t token)
{
    if (s_active) return;                           /* la RPi repite la orden 3 veces */
    snprintf(s_url, sizeof(s_url), "http://%s:%u/fw/ota.bin?t=%016llx", rpi_ip, (unsigned)port, (unsigned long long)token);
    s_size = size; s_active = true;
    ESP_LOGW(TAG, "OTA: descargando %u bytes de %s:%u", (unsigned)size, rpi_ip, (unsigned)port);
    if (xTaskCreate(ota_task, "ota", 8192, NULL, 5, NULL) != pdPASS) { s_active = false; ESP_LOGE(TAG, "sin memoria para la tarea OTA"); }
}

void ota_mark_valid(void)
{
    const esp_partition_t* run = esp_ota_get_running_partition();
    esp_ota_img_states_t st;
    if (run && esp_ota_get_state_partition(run, &st) == ESP_OK && st == ESP_OTA_IMG_PENDING_VERIFY) {
        if (esp_ota_mark_app_valid_cancel_rollback() == ESP_OK) ESP_LOGI(TAG, "firmware nuevo confirmado (rollback cancelado)");
    }
}
