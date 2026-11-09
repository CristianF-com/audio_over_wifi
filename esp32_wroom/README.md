# Receptor para ESP32-WROOM-32D (ESP32 clásico, solo WiFi 2.4 GHz)

Repuesto del XIAO ESP32-C5: si falta un C5 puedes usar un ESP32 normal. **Usa el mismo código** que `../esp32` (los fuentes se compilan desde ahí); esta carpeta solo cambia el chip, los pines y la configuración.

> **Estado:** compila y se flashea con ESP-IDF real, y la RPi lo detecta por Bluetooth y le configura el WiFi (confirmado por el usuario). El audio y la latencia Auto funcionan en placa (confirmado por el usuario). La actualización por WiFi (OTA) también funciona (confirmado por el usuario). Si algo falla, copia el error.

## Diferencias con el C5
| | XIAO ESP32-C5 | ESP32-WROOM-32D |
|---|---|---|
| WiFi | 2.4 y 5 GHz | **solo 2.4 GHz** |
| Pines I2S (BCK / WS / DOUT) | GPIO8 / 9 / 10 (D8 / D9 / D10) | **GPIO26 / 25 / 22** |
| ESP-IDF | ≥ 5.5 | ≥ 5.1 |
| Núcleos | 1 | 2 (el código usa tareas sin fijar núcleo) |

El router dedicado debe emitir también en **2.4 GHz** (un SSID propio para los ESP32 clásicos; el C5 puede seguir en 5 GHz con otro). Con 2.4 GHz usa un canal fijo y despejado (1, 6 u 11), ancho 20 MHz. El país del WiFi es `AR` (Kconfig `SONOS_WIFI_COUNTRY`).

## Cableado PCM5102A
`BCK → GPIO26`, `LCK (WS) → GPIO25`, `DIN → GPIO22`, `VIN → 3V3`, `GND → GND`, y en el módulo `SCK → GND` (usa su PLL interno). Si tu módulo tiene los puentes FLT/DEMP/XSMT/FMT, déjalos como en la versión del C5.

## Compilar y grabar
```bash
. ~/esp/esp-idf/export.sh                 # tu ruta de ESP-IDF
cd esp32_wroom
rm -f sdkconfig                           # solo si antes compilaste otro chip en esta carpeta
idf.py set-target esp32
idf.py menuconfig                         # SONOS receiver: SSID, contraseña, ID, IP de la RPi (opcional)
idf.py -p /dev/ttyUSB0 flash monitor
```
Después se configura igual que el C5: ID y WiFi por Bluetooth desde el panel («Añadir altavoces») o con `scripts/provision_esp.sh`. El ID (0–9) es el que identifica al altavoz en el panel: si reemplazas un C5, grábale **el mismo ID** y hereda su configuración (canales, volumen, posición en el plano).

## Si da problemas
* Si el Bluetooth falta de memoria junto al WiFi: en `menuconfig`, `Component config → Bluetooth → NimBLE`, baja las conexiones; o desactiva `SONOS_BLE_ENABLE` y configura el WiFi por Kconfig/NVS.
* El ESP32 clásico tiene un solo ADC/GPIO sensible al arranque: GPIO22/25/26 no son pines de arranque, pero evita mover los pines a GPIO0/2/12/15.

## Actualización por WiFi (OTA)
Comparte `ota_update.c` con el C5. `partitions.csv` define dos aplicaciones de 1,5 MB y `sdkconfig.defaults` fija flash de 4 MB (los WROOM-32D suelen traer 4 MB; con menos no cabe). La primera vez hay que flashear por cable. La OTA funciona en placa (confirmado por el usuario); el rollback ante un firmware defectuoso no se ha provocado a propósito.
