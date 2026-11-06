# SonosWiFi — instrucciones para trabajar en este repo

Audio multicanal inalámbrico: Traktor Audio 10 (USB, 10 canales) → Raspberry Pi 400 (C++, UDP unicast) → hasta 10 XIAO ESP32-C5 con PCM5102A. Uso principal: ver películas (lip-sync con el desfase de audio del reproductor).

## Por dónde empezar
* Lee `PROYECTO_RESUMEN.md` antes que el código: estado, protocolo, historial de versiones y qué NO está verificado.
* `README.md` explica instalación, Wi-Fi, Bluetooth, calibración y pruebas.
* El usuario escribe en español: responde, comenta y escribe textos de interfaz en español.

## Pruebas (sin hardware)
* Batería completa: `bash tests/run_tests.sh` (necesita g++, `libasound2-dev`, `libssl-dev`, node, python3; lua5.4 opcional).
* Interfaz (Playwright): `bash tests/run_ui_tests.sh [carpeta_capturas]` (necesita `playwright` global y `/tmp/sonos_harness` compilado por `run_tests.sh`).
* No dejes un `sonos_harness` en marcha entre ejecuciones: `pkill -x sonos_harness`.
* Todo cambio debe dejar la batería en verde y añadir pruebas.

## Páginas web
* Las fuentes están en `rpi/web/` (`panel.html`, `calibrate.html`, `ble.html`, `shared.css`, `analysis.js`).
* `rpi/include/web_pages.h` es GENERADO: tras editar `rpi/web/`, ejecuta `python3 scripts/gen_web_pages.py`. `tests/check_pages_js.sh` falla si no está al día.
* Estilo: tema oscuro por defecto (fondo negro `#000`, acento azul `#4aa3ff`) y temas opcionales Grafito y Claro suave (nunca blanco puro); sin morado; interfaz en español.

## Reglas del proyecto
* Protocolo en `common/protocol.h` (versión 5). Si cambia, hay que reflashear todos los ESP: actualiza `PROTOCOL_VERSION`, el firmware y los tests.
* El procesado (crossover y EQ) está en pass-through por defecto; se activa con `POST /api/dsp`.
* El firmware NO se ha compilado con ESP-IDF real ni probado en hardware, y el Bluetooth desde la RPi (`scripts/sonos_ble_prov.py`) solo se ha probado simulado. Dilo siempre que se afirme que algo funciona "de verdad".
* La WiFi de los altavoces se guarda en `config.txt.ble` (0600); nunca la devuelvas por la API ni la subas al repo.
* Tema: variables en `shared.css` (`:root` oscuro, `html[data-theme=graphite]`, `html[data-theme=light]`); no pongas colores fijos en las páginas, usa variables. `rpi/web/theme.js` se incrusta con `/*@THEME_JS@*/`. Posiciones del plano: por canal (`pos_x/pos_y` izquierdo, `pos2_x/pos2_y` derecho; −1 = automática), solo visuales.
* PWA: `rpi/web/{manifest.webmanifest,sw.js,icon.svg,icons/}` los sirve `web_server.cpp` sin auth; `gen_web_pages.py` los incrusta (hash de build en el nombre de caché). Iconos: `python3 scripts/gen_icons.py` (PNG opacos). `/api/*` nunca se cachea. Mantén objetivos táctiles ≥44 px, contraste ≥4,5:1 (bordes ≥3:1) y respeta áreas seguras; ver `AUDITORIA_UX.md`.
