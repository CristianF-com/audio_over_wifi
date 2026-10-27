#pragma once
/**
 * ble_prov — configuración del receptor por Bluetooth LE (NimBLE), sin tocar el Wi-Fi.
 *
 * Servicio GATT propio (UUID base 5c0a XXXX -8e3b-4d5e-9a41-534f4e4f5346; XXXX = 0001..0006):
 *   0001 servicio
 *   0002 SSID      R/W   texto UTF-8, 1..32 B
 *   0003 PASS      W     texto, 8..63 B (0 B = mantener la actual). Nunca se puede leer.
 *   0004 ID        R/W   1 byte, 0..9
 *   0005 STATUS    R/N   JSON (id, wifi, ip, rssi, rpi, sync, ...)
 *   0006 COMMAND   W     "apply" | "identify" | "reboot" | "factory"   (ASCII)
 *   0007 RPI_IP    R/W   IPv4 de la RPi en texto (vacío = descubrimiento por broadcast)
 * Escribir SSID/PASS/ID solo los deja en RAM; "apply" los guarda en NVS y reinicia.
 */
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

typedef struct {
    int    (*save)(const char* ssid, const char* pass /* NULL = mantener */, uint8_t id, const char* rpi_ip);  /* 0 = OK */
    void   (*identify)(void);                      /* pitido por el altavoz */
    void   (*reboot)(void);                        /* reinicio diferido */
    int    (*factory)(void);                       /* borra NVS "config"; 0 = OK */
    size_t (*status_json)(char* buf, size_t len);  /* devuelve bytes escritos */
} ble_prov_ops_t;

void ble_prov_init(const ble_prov_ops_t* ops, const char* cur_ssid, uint8_t cur_id, const char* cur_rpi_ip);
void ble_prov_stop(void);          /* deja de anunciarse (la radio BLE queda en silencio) */
void ble_prov_resume(void);        /* vuelve a anunciarse */
void ble_prov_notify(void);        /* avisa de cambio de STATUS a los suscritos */
bool ble_prov_connected(void);
