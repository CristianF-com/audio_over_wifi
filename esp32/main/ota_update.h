#pragma once
/* Actualización de firmware por WiFi (OTA). La RPi manda PKT_OTA por UDP con el puerto HTTP, el tamaño y un token;
 * aquí se descarga GET http://<RPi>:<puerto>/fw/ota.bin?t=<token> a la otra partición de aplicación y se reinicia.
 * ¡SIN COMPILAR NI PROBAR con ESP-IDF real ni en hardware! */
#include <stdint.h>
#include <stdbool.h>

void ota_update_start(const char* rpi_ip, uint16_t port, uint32_t size, uint64_t token);  /* no hace nada si ya hay una en curso */
bool ota_update_active(void);   /* true mientras descarga: el resto de tareas debe callar (HELLO, SYNC, REPORT) */
void ota_mark_valid(void);      /* confirma el firmware nuevo (cancela el rollback); no hace nada si no estaba pendiente */
