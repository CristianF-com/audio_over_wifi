# PROYECTO_RESUMEN.md — contexto para futuras iteraciones (v6)

> Leer ESTE archivo en lugar de releer todo el código. Si cambias algo, actualiza la sección
> "Estado" y "Historial".

## 1. Qué es
Sistema de audio multicanal inalámbrico. **Traktor Audio 10** (USB, ALSA `S32_LE`, 10 ch, 48 kHz)
→ **Raspberry Pi 400** (C++17, `sonos_wifi`) → UDP/WiFi → hasta **5× Seeed XIAO ESP32-C5** (C, ESP-IDF ≥5.5)
→ I2S 16-bit → **PCM5102A**. Cada ESP reproduce un par estéreo de canales (L/R). Web UI en RPi `:8080`.

## 2. Estructura
```
common/protocol.h            ÚNICA definición del protocolo (RPi y ESP lo incluyen)
rpi/CMakeLists.txt           include_directories(include ../common); sin dependencias salvo ALSA
rpi/include/*.h, rpi/src/
  main.cpp                   args -d/-p/-P/-S/-C/-K, arranque, heartbeat; sale con rc=1 si la captura muere (systemd reinicia)
  audio_capture.cpp          ALSA S32_LE 10ch, período 240 fr (5 ms), hilo SCHED_FIFO 80, findCard("traktor")
  udp_streamer.cpp           control UDP :5005 (HELLO/REPORT/SYNC), pushFrames() hot path, config de nodos
  web_server.cpp             HTTP :8080 + HTTPS :8443 (OpenSSL, opcional WITH_TLS), SPA embebida, API REST validada (sin excepciones, sin CORS)
  web_pages.h                PAGE_CAL (/calibrate) y PAGE_BLE (/ble); el análisis de audio está entre /*ANALYSIS_BEGIN*/ y /*ANALYSIS_END*/
esp32/main/receiver_main.c   firmware único (4 tareas FreeRTOS); Kconfig.projbuild; sdkconfig.defaults
esp32/main/ble_prov.c/.h     servicio GATT NimBLE de configuración (SSID/clave/ID/estado/comandos)
scripts/build.sh             instala deps, compila, crea servicio systemd
scripts/provision_esp.sh     NVS (id, ssid, pass) por placa, sin recompilar
tests/run_tests.sh           harness.cpp + fake_esp.py (~60 checks, incl. HTTPS y calibración) + test_analysis.js (Node) ; tests/sim_sync.py ; tests/check_pages_js.sh
```

## 3. Protocolo v5 (little-endian, `#pragma pack(1)`, magic `0x534E4657`)
Puertos: **5005** control (RPi escucha; cada ESP también), **5006** audio (cada ESP).
| Tipo | Dir | Tamaño | Notas |
|---|---|---|---|
| HELLO 0x01 | ESP→bcast:5005 cada 2 s (+unicast a la IP fija si está configurada) | 16 | esp_id, version=3, audio_port |
| WELCOME 0x02 | RPi→ESP | 20 | ch L/R, vol, enabled, rate, playout_delay_us. Respuesta a cada HELLO (= keepalive) |
| AUDIO 0x03 | RPi→ESP:5006 | 500 | hdr 20 B + 120 frames stéreo s16. `timestamp_us` = hora maestra del 1er frame |
| REPORT 0x04 | ESP→RPi cada 2 s | 48 | offset, rtt, jitter, buffer_frames, sync_err, lost, late, underruns, drift_ppm |
| CONFIG 0x05 | RPi→ESP:5005 | 16 | cambio desde web UI (incl. enabled) + `playout_delay_us` global (0 = sin cambio) |
| SYNC_REQ 0x06 / SYNC_RESP 0x07 | ESP↔RPi | 16 / 32 | t1 (ESP), t2/t3 (RPi). offset = ((t2−t1)+(t3−t4))/2 |

Reloj maestro = `CLOCK_MONOTONIC_RAW` de la RPi (µs). **Volumen solo en la RPi** (ganancia = vol/255 × master).
Constantes clave (`protocol.h`): `FRAMES_PER_PACKET=120`, `PLAYOUT_DELAY_US=80000` (defecto; rango `PLAYOUT_MIN_US=40000`..`PLAYOUT_MAX_US=200000`), `RX_QUEUE_PACKETS=96` (240 ms ≥ playout máx.), `MAX_NODES=10` (v7). **v5 (v8)**: `PKT_NACK=0x09` (ESP→RPi: `NackPacket` 40 B, hasta `NACK_MAX_SEQS=8` seq), `ReportPacket` 52 B (+`recovered_packets`), `TX_HISTORY_PACKETS=128`. **v4 añade `PKT_AUDIO_MONO=0x08`** (mismo tamaño 500 B, 240 frames mono = 5 ms; el ESP lo expande a 2 trozos estéreo L=R en `audio_rx_task`, así cola/PLC no cambian).

## 3b. Calibración acústica (RPi + móvil) — NUEVO
* `UdpStreamer::calStart(sequence, level_db)` → el hilo de captura **sustituye todo el audio por silencio + chirps**. Constantes en `udp_streamer.h`:
  `CAL_SLOT_US=1.4 s`, `CAL_CHIRP_US=0.3 s`, `CAL_LEAD_US=1.5 s`, `CAL_TAIL_US=0.6 s`, chirp log 250→12000 Hz, fade coseno 10 ms. Mono, sin volumen ni `latency_offset`; ts = tiempo de stream exacto.
* API: `POST /api/cal/start {level_db(-40..-3), rounds(1..3)}` → `{sequence, slot_ms, chirp_ms, lead_ms, fade_ms, f0, f1, total_ms}` (409 si no hay nodos online+habilitados o ya corre); `POST /api/cal/stop`; `GET /api/cal/status`.
* Latencia ahora en **µs**: `ESPNode.latency_offset_us`, `/api/calibrate {esp_id, latency_us | latency_ms}` (±500000).
* `/calibrate` (HTTPS, getUserMedia sin AEC/NS/AGC): graba, correlación cruzada por FFT (overlap-save 64k), detecta la rejilla de chirps (score Σlog), refina con parábola (<1 sample), mide nivel (potencia − ruido).
  `d_k = (pos_k−pos_0) − k·slot` → `latencia_k = max(d) − d_k`; `volumen_k = 255·10^((Lmin−L_k)/20)`. **`expect_s`** (instante de la petición + lead) evita que un nodo mudo desplace la secuencia un slot.
* Precisión probada en Node: retardos < 15 µs, niveles < 0.1 dB (sintético, 44.1 kHz, ruido + eco). Deriva del reloj del móvil (~20 ppm × 20 s) ≈ 0.4 ms máx.; no probado con móvil real.

## 3c. BLE (ESP32-C5) — NUEVO
* Servicio `5c0a0001-8e3b-4d5e-9a41-534f4e4f5346`; chars 0002 SSID R/W, 0003 PASS W, 0004 ID R/W, 0005 STATUS R/N (JSON), 0006 CMD W (`apply|identify|reboot|factory`). `apply` guarda en NVS ("config": wifi_ssid, wifi_pass, receiver_id) y reinicia a los 0.8 s.
* Nombre `SonosWiFi-<últimos 2 bytes MAC BT>`. Escrituras exigen cifrado (Just Works, `CONFIG_SONOS_BLE_REQUIRE_ENC`). BLE activo `SONOS_BLE_WINDOW_S`=300 s tras el arranque y siempre sin Wi-Fi; `report_task` lo apaga con Wi-Fi activo y lo reactiva si el Wi-Fi cae >60 s.
* Orden de arranque: i2s → tarea i2s (pitido `identify` sin red) → BLE → wifi_start (no bloqueante, reintento con backoff 1–10 s por esp_timer) → wifi_wait → sockets → tareas.
* Acceso: `https://<rpi>:8443/ble` (Web Bluetooth: Chrome/Edge; iPhone: Bluefy) o nRF Connect.
* `bt` + NimBLE en `sdkconfig.defaults`; los nombres CONFIG_BT_NIMBLE_* pueden variar según versión de IDF (revisar con menuconfig).

## 3d. Mejoras v6
* **Playout en caliente:** `UdpStreamer::setPlayoutUs()` (40–200 ms) → `sendConfig()` a los nodos online; WELCOME también lo lleva. API `POST /api/playout {ms}`; `GET /api/status` → `playout_ms/min/max`. En el ESP `g_playout_us` se actualiza al recibir CONFIG y `i2s_task` reconverge solo (descarta si baja, silencio si sube).
* **Persistencia RPi:** `-c /var/lib/sonos_wifi/config.txt` (systemd `StateDirectory=sonos_wifi`). `key=value`: `master_volume`, `playout_us`, `nodeN.{ch_left,ch_right,volume_l,volume_r,enabled,latency_offset_us}`. Guardado diferido ≥1 s en el hilo de control + al apagar; `rename()` atómico; valores inválidos o claves desconocidas se ignoran.
* **PLC (ESP, `i2s_task`):** al hacer pop de un paquete, si `ts − last_ts − 2500 µs` indica 1..8 paquetes perdidos, escribe k chunks (1.º = último audio con fade-out lineal, resto silencio), avanza `last_ts` y aplica fade-in al paquete real. Se desactiva (`have_last=false`) tras descartes por tardío / silencios por temprano / underrun, para no deshacer el catch-up. Contador `plc=` en el log. Simulado: con 1 % de pérdida, sin PLC el error llega a −4.96 ms; con PLC ±0.1 ms.
* **Contraseña (opcional):** HTTP Basic, usuario libre, comparación en tiempo constante; fichero `/etc/sonos_wifi/password` (o `-W`), `scripts/set_password.sh`. Sin límite de intentos (usar HTTPS y LAN de confianza).
* **IP fija de la RPi:** NVS `rpi_ip` / Kconfig `SONOS_RPI_IP` / BLE char `…0007`; HELLO unicast extra. Alternativa sin dependencias al mDNS.
* **Decisiones de NO hacer (y por qué):** *OTA* (requiere tabla de particiones con 2 slots + descarga firmada; sin placa para probarlo puede dejar nodos inservibles; flashear 5 placas por USB es viable); *mDNS* (añade el componente externo `espressif/mdns` + avahi; el broadcast ya funcionó y la IP fija cubre el caso filtrado); *pacing del envío* (el AP serializa por estación igualmente; beneficio despreciable frente al coste en el hilo RT); *FEC/retransmisión* (añadirían latencia; el PLC cubre pérdidas ocasionales).

## 4. Sincronización (diseño)
* **RPi**: `pushFrames` mantiene un reloj de stream suave (sigue al reloj de pared con ganancia 0.002; re-ancla si >20 ms),
  acumula frames por nodo en paquetes de exactamente 120, `ts = ts_período + f·1e6/48000 + latency_offset_ms·1000`.
* **ESP `control_task`**: SYNC_REQ cada 150 ms (12 primeros) y luego 1 s; ventana de 8, usa la de menor RTT; si el offset salta >100 ms (RPi reiniciada) se reinicia la ventana.
* **ESP `i2s_task`**: `err = master_now + DMA_LAT_US − (ts + playout)` (+ = tarde).
  `err>5 ms` descarta paquete · `err<−5 ms` escribe silencio · en medio: IIR (1/8) y 1 frame descartado/repetido cada 4 chunks si `|err_f|>100 µs` (máx ≈2083 ppm).
  Resultado simulado: error estable ±100 µs por nodo (≤~200 µs entre nodos), sin descartes duros.
* `DMA_LAT_US` = (6·120 − 60) fr ≈ 13.75 ms es una **estimación común a todos los nodos** (no afecta a la sincronía mutua); calibrar con micrófono vía "Ajuste de latencia".

## 5. Específico XIAO ESP32-C5
* 1 solo núcleo → `xTaskCreate` (prioridades: ctrl 12, rx 14, i2s 15, report 5; WiFi 23, tcpip 18). **Nada de `PinnedToCore(...,1)`**.
* Sin APLL → `I2S_CLK_SRC_DEFAULT`; el desvío de reloj se corrige por software (sección 4).
* Pines I2S por defecto D8=GPIO8 (BCK), D9=GPIO9 (WS), D10=GPIO10 (DOUT); configurables por Kconfig. D2=GPIO25 y D3=GPIO7 son strapping (evitar). Pinout tomado del wiki de Seeed — **verificar contra esquemático**.
* **Banda**: Kconfig `SONOS_WIFI_BAND` (5G por defecto / AUTO / 2G) → `apply_band_mode()` en `WIFI_EVENT_STA_START` antes de `esp_wifi_connect()` (la API exige llamarla tras `esp_wifi_start`; el C5 no hace dual-band simultáneo). `sae_pwe_h2e=WPA3_SAE_PWE_BOTH` para routers solo-WPA3. Canal/banda en el log y en el JSON BLE (`"ch"`).
* `esp_wifi_set_ps(WIFI_PS_NONE)`, `WIFI_ALL_CHANNEL_SCAN`, reintento WiFi sin límite, `CONFIG_FREERTOS_HZ=1000`, `LWIP_UDP_RECVMBOX_SIZE=32`.
* Target: `idf.py set-target esp32c5`. Si el compilador rechaza `auto_clear_after_cb` (IDF<5.4) usar `auto_clear`.

## 6. RPi: puntos a recordar
* Hilos RT desde el código (captura 80, control 60); el servicio systemd NO fuerza FIFO global (evita que web/otros hilos hereden RT). `AmbientCapabilities=CAP_SYS_NICE`.
* `-d auto` localiza la tarjeta por nombre "traktor" (ALSA `snd_card_get_name`); el índice hw:N cambia entre arranques.
* Si captura muere (USB desconectado) el proceso sale con 1 → systemd `Restart=always`.
* Web: máx. 16 conexiones, body ≤16 KB, parseo con `strtol/strtod` (400 en entradas inválidas), sin CORS (API sin auth → solo LAN confiable).
* Orden de dirección: `UdpStreamer::nodes_` es `std::array<ESPNode,5>` (antes `std::map`).

## 7. Estado de verificación
| Parte | Verificado | Cómo |
|---|---|---|
| RPi compila (`-Wall -Wextra`, sin warnings) | ✅ | cmake+g++ con libasound2-dev real |
| RPi protocolo/audio/web/HTTPS/calibración/persistencia/auth/NACK/graves/EQ/lip-sync | ✅ (~270 checks en total) | `tests/run_tests.sh` (ESP simulado en 127.0.0.2; chirps verificados: separación 1400.000 ms, −12 dBFS exactos, silencio fuera) |
| Análisis de calibración (JS) | ✅ en Node | `tests/test_analysis.js` (sintético + señal real generada por el C++) |
| JS de las páginas | ✅ sintaxis | `tests/check_pages_js.sh` |
| Controlador de sync + PLC + cambio de playout | ✅ en simulación | `tests/sim_sync.py` (±150 ppm, offsets hasta 20 ms, 1–6 % pérdidas, 80→120 / 80→50 ms) |
| Firmware: validador IPv4 y UUID C↔JS | ✅ | `tests/test_firmware_units.py` |
| Firmware: reordenación/NACK + tarea RX real (PC, loopback) | ✅ | `tests/test_reorder.c`, `tests/test_esp_rx.c` |
| Firmware ESP32-C5 (incl. BLE) | ⚠️ solo sintaxis con stubs | **no compilado con ESP-IDF real ni probado en placa** (NimBLE/pairing sin probar) |
| Calibración con móvil real / Web Bluetooth real | ❌ no probado | certificado autofirmado en iOS/Android, permisos de micrófono, pairing |
| ALSA con Traktor real | ❌ no probado | sin hardware |
| Audio/sync reales en WiFi | ❌ no probado | el plazo de reenvío y el colchón mínimo hay que ajustarlos en la red real (contadores `perd`/`rec`/`tarde`) |
| EQ propuesto con móvil/altavoces reales | ❌ no probado | solo con respuestas sintéticas conocidas (error < 1 dB) |
| Script mpv en mpv real | ⚠️ | probado con un mpv simulado contra la API real, no con mpv |

Primer paso al volver: compilar `esp32/` con ESP-IDF 5.5+; arreglar lo que salga (APIs I2S/WiFi del C5).

## 8. Limitaciones conocidas / ideas
* Latencia total fija ≈ 80 ms + DMA (no sirve para monitorizar en vivo).
* Ráfagas: 2 paquetes por nodo cada 5 ms; sin pacing en el envío (si hay pérdidas en ráfaga, repartirlas con un hilo de envío).
* Sin FEC/retransmisión: pérdida = hueco corto (el ESP no oculta pérdidas; se oye un corte).
* Reloj de la Traktor vs. RPi: el stream sigue al reloj de pared; la deriva (decenas de ppm) la absorbe el servo de la RPi y el slew del ESP.
* Calibración: solo valida nodos de rango completo (250 Hz–12 kHz); requiere la Traktor conectada (el reloj sale de la captura); un solo punto de escucha.
* Contraseña opcional (sin límite de intentos); sin persistencia del estado «online» (obvio). El PLC solo oculta ≤8 paquetes seguidos (20 ms); más pérdida → resincronización por descartes/silencios.
* Posibles mejoras restantes: OTA, mDNS, medir en placa y bajar el playout por defecto si `late`=0, selector de playout por nodo, ecualización por nodo desde la calibración.

## 9. Historial
* **v9.1**: configuración de ESP32 por Bluetooth desde la Raspberry (ver README): `scripts/sonos_ble_prov.py`, `rpi/src/ble_manager.cpp`, API `/api/ble/*`, panel «Añadir altavoces». `jsonGet` ahora decodifica `\\uXXXX` y `\\n`. Sin cambios de firmware. Tests: 343 de la batería + UI. **Sin probar con Bluetooth real.**
* **v9**: interfaz web rediseñada y pass-through de crossover/EQ (ver README, «Interfaz web y pass-through»). Sin cambios de protocolo ni de firmware: **no hace falta reflashear**. Tests: batería completa + 36 pruebas de UI.
* **v8** (protocolo v5, FW_VERSION 5): (1) **Reenvío**: `esp32/main/reorder.h` (C puro: ventana de 16, NACK, 2.º intento a mitad de espera, plazo = clamp(playout/4, 8, 20 ms), resync si el seq salta >16 adelante o >64 atrás) usado por `audio_rx_task`; RPi guarda historial por nodo (`hist_`, mutex por nodo) y atiende `PKT_NACK` en `handleNack` (solo desde la IP del nodo, tope 300/s). (2) **DSP en la RPi** (`rpi/include/dsp.h`): crossover LR4 (HP satélites / LP sub + suma de graves de satélites; solo si hay un SUB habilitado y online, si no los satélites van completos), EQ de 6 bandas por nodo (`EQ_EDGES_HZ` 250..12000, peaking Q=1.4), roles `ROLE_FULL/SAT/SUB`; todo omitido en calibración; persistencia `xover_hz`, `lipsync_extra_ms`, `nodeN.role`, `nodeN.eqB`. `setNodeConfig` solo pasa a layout CUSTOM si cambia canal/mono/enabled (no por volumen). (3) **/calibrate**: `analyze()` devuelve `bands`/`band_ok` (FFT por bandas, ruido por banda) y `propose(nodes, volIncl, eqIncl)` calcula EQ = media del conjunto − respuesta propia − media de bandas, ±6 dB; subs desmarcados por defecto. (4) **Lip-sync**: `GET/POST /api/latency` (`mpv_audio_delay_s`, `extra_ms`), `scripts/sonos_lipsync.lua`. API nueva: `/api/bass`, `/api/eq`, `/api/latency`; `/api/node` acepta `role`. Panel: tarjetas Gestión de graves y lip-sync, rol y EQ por nodo, contador `rec`. **Tests: 269 PASS** (nuevos: test_reorder.c, test_esp_rx.c — extrae la tarea RX REAL de receiver_main.c y la ejecuta en el PC con loopback—, test_dsp.cpp, test_lipsync.lua, ampliados fake_esp.py/test_config.cpp/test_analysis.js). ESP-NOW descartado por el usuario (no soporta tantos canales). **Hay que reflashear todos los ESP.**
* **v7**: hasta 10 ESP (IDs 0–9). Cada nodo tiene flag `mono` (RPi: `ESPNode.mono`, persistido como `nodeN.mono`; `setNodeConfig(..., mono)` fuerza ch_right=ch_left y vol_r=vol_l). Layouts `LAYOUT_STEREO5/MONO10/CUSTOM` (`setLayout`, `layout=` en config, `/api/layout`); editar un nodo → CUSTOM. Defaults: nodos 0–4 estéreo pares, 5–9 deshabilitados. Protocolo v4, FW_VERSION 4. Panel: selector «Salida» por ESP + botones de distribución, nodos en rejilla de 2 columnas, nodos 5–9 ocultos mientras estén offline y deshabilitados en estéreo5. Tests: 151 PASS. **Hay que reflashear todos los ESP.**
* **v6**: latencia ajustable en caliente (protocolo v3), PLC con fades en el ESP, persistencia de configuración en la RPi, contraseña opcional de la web, IP fija de la RPi (BLE/NVS/Kconfig). **Hay que reflashear todos los ESP.**
* **v5.1**: banda Wi-Fi fijable (5 GHz por defecto), WPA3, canal visible. El usuario usará **5 GHz con router Wi-Fi 6, RPi por Ethernet**. Su versión anterior (v3, ESP32 clásico) funcionó bien en 2.4 GHz con ≥85 ms de buffer real (ring de 341 ms, arranque al 25 %, deriva 30–70 %); por eso `PLAYOUT_DELAY_US` queda en 80 ms y bajarlo no está validado. Pendiente opcional: selector de playout en caliente (60/80/120 ms). Uso previsto: ver películas → compensar con el desfase de audio del reproductor (-80 ms).
* **v5**: calibración acústica con móvil (chirps + /calibrate + latencia en µs), HTTPS autofirmado, configuración BLE (GATT NimBLE + página Web Bluetooth `/ble`), backoff Wi-Fi por timer, pitido de identificación.
* **v3 (original, ESP32 clásico)**: GPIO 26/25/22, APLL, núcleo 1.
* **v4 (esta)**: portado a XIAO ESP32-C5 y RPi 400. Corregido: `protocol.h` triplicado y include roto (`../protocol.h`), `handleReport` nunca llamado, volumen aplicado dos veces, inserción de drift de 128 frames (clic), `PKT_CONFIG` al puerto equivocado, lectura parcial de ringbuf, `std::stoi` sin try (crash) y canales sin validar (lectura fuera de buffer), IP de RPi no actualizada, offline solo al llegar HELLO, WiFi sin reintento infinito, sin `WIFI_PS_NONE`, `SO_RCVBUF` inútil en lwIP, `build.sh` detectaba con `aplay -l`, `stop()` cerraba PCM con hilo leyendo, systemd forzaba FIFO a todos los hilos, CORS `*` sin auth, README con comandos inexistentes (`idf.py nvs-flash`). Añadido: sincronía real (SYNC + playout fijo), paquetes de tamaño fijo 120 fr, `-d auto`, tests.

## v6.1 — Estilo web unificado
- Panel principal (`web_server.cpp`), `/calibrate` y `/ble` (`web_pages.h`) comparten paleta: fondo negro `#000`, tarjetas `#0d1016`, acento azul `#4aa3ff`, ok `#3ecf8e`, error `#ff6b6b`. Sin morado.

## v7 — Un ESP por canal
Ver sección «Historial v7». Archivos tocados: common/protocol.h, rpi/{udp_streamer.h/.cpp, web_server.h/.cpp, web_pages.h}, esp32/main/{receiver_main.c, Kconfig.projbuild, ble_prov.h}, scripts/provision_esp.sh, tests/{fake_esp.py,test_config.cpp}. Pendiente/no verificado: firmware sin compilar con ESP-IDF real ni probado con 10 placas; capacidad Wi-Fi con 10 ESP no medida (airtime ≈ el de 5 estéreo).
