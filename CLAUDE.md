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
* Estilo: tema **Grafito por defecto** (fondo `#1c1c1e`; «Automático» con el sistema oscuro también da Grafito), temas opcionales Oscuro (negro `#000`, acento azul `#4aa3ff`) y Claro suave (nunca blanco puro); se elige en Sistema → Interfaz; sin morado; interfaz en español. El nombre que se muestra en la web es **AStream** (el nombre BLE de los ESP sigue siendo `SonosWiFi-XXXX`). Excepción de diseño pedida por el usuario: en «Claro» el acento `--ac` es blanco (`--ac-bg: rgb(0 0 0 / 24%)`, interruptor activo en `--mut`), por lo que ahí el acento no cumple 4,5:1 (la prueba lo exceptúa).

## Reglas del proyecto
* Protocolo en `common/protocol.h` (versión 5). Si cambia, hay que reflashear todos los ESP: actualiza `PROTOCOL_VERSION`, el firmware y los tests. Las ampliaciones compatibles (`HELLO.reserved`, `REPORT.rssi_dbm`, `PKT_OTA`, ID automático: `ESP_ID_AUTO`=255 en `HELLO.esp_id` y `WELCOME_F_ASSIGNED`) no suben la versión. El ESP no necesita ID: la RPi lo asigna por MAC (`ESPNode.mac`, persistido como `nodeN.mac`); el ESP lo guarda solo en RAM.
* El procesado (crossover y EQ) está en pass-through por defecto; se activa con `POST /api/dsp`.
* El firmware del **ESP32-C5 compila y se flashea** con ESP-IDF real (confirmado por el usuario, v9.7); el **WROOM no se ha compilado ni probado**. El comportamiento en placa (audio, sync, OTA, BLE) solo está confirmado si el usuario lo dice: lo demás sigue sin verificar, y el Bluetooth desde la RPi (`scripts/sonos_ble_prov.py`) solo se ha probado simulado. Dilo siempre que se afirme que algo funciona "de verdad".
* La WiFi de los altavoces se guarda en `config.txt.ble` (0600); nunca la devuelvas por la API ni la subas al repo.
* Tema: variables en `shared.css` (`:root` oscuro, `html[data-theme=graphite]`, `html[data-theme=light]`); no pongas colores fijos en las páginas, usa variables. `rpi/web/theme.js` se incrusta con `/*@THEME_JS@*/`. Posiciones del plano: por canal (`pos_x/pos_y` izquierdo, `pos2_x/pos2_y` derecho; −1 = automática), solo visuales.
* PWA: `rpi/web/{manifest.webmanifest,sw.js,icon.svg,icons/}` los sirve `web_server.cpp` sin auth; `gen_web_pages.py` los incrusta (hash de build en el nombre de caché). Iconos: `python3 scripts/gen_icons.py` (PNG opacos). `/api/*` nunca se cachea. Mantén objetivos táctiles ≥44 px, contraste ≥4,5:1 en texto (bordes de campos e interruptores ≥3:1; los botones llevan etiqueta) y respeta áreas seguras; ver `AUDITORIA_UX.md`.
* Firmware: `esp32/` (XIAO ESP32-C5) y `esp32_wroom/` (ESP32 clásico, 2.4 GHz) comparten los fuentes de `esp32/main`; solo difieren `sdkconfig.defaults`, `Kconfig.projbuild` y el CMake de `main`. No rompas el C5 al tocar el código común (usa `#if CONFIG_IDF_TARGET_ESP32C5`).
* Navegación (v9.11): ≤ 760 px barra inferior fija pegada a los bordes; móvil horizontal (`orientation:landscape` y alto ≤ 500 px) riel izquierdo, cabecera estática en una fila con botones de icono (excepción a «los botones llevan etiqueta»: llevan `aria-label`/`title`). `.top` es sólido (sin blur ni degradado). Modales a pantalla completa en horizontal, contenido en rejilla (`.dbody`, `.wide` = fila entera). Sonido: selector de modo + subpestañas (`#snd_sub`, `.sbwrap` con flechas ‹ ›); en pass-through solo se ve `#mode_pass`.
* Forma de la interfaz: por defecto **recta** (`data-shape=square`: todos los radios a 0, salvo los altavoces, vúmetros y «Escucha» del radar). La variante redonda (`?forma=redonda`, se guarda en `sonos_shape`; `?forma=recta` vuelve) usa píldoras (`border-radius:999px`) en botones/seg/pestañas, `--r-card` (24 px) en tarjetas, `--r-sheet` (28 px) en hojas. Tipografía: Roboto autoalojada (`/fonts/roboto-*.woff2`, sin CDN). Los deslizadores usan `--tw/--th/--rh`; los del EQ son más chicos (30×18) para no pisarse.
* Nombres de las 10 entradas: `UdpStreamer::channelNames()` (fichero `<config>.chnames`), `/api/names` y `ch_names` en `/api/status`; en el panel `CH[]` se actualiza en el sitio con `setChNames`; no uses `CH` fijo en HTML nuevo. El plano con zoom no debe atrapar el scroll de la página: `touch-action` queda en `pan-y` y `touchmove` decide (`canPanY`).
* OTA por WiFi: `POST /api/firmware` (hasta 3 MB, solo ese endpoint), `POST /api/ota`, y `GET /fw/ota.bin?t=<token>` público protegido por token; el firmware (`esp32/main/ota_update.c`) y `partitions.csv` compilan en el C5 (confirmado por el usuario), pero la OTA en sí (descarga, rollback, particiones) sigue SIN probarse en placa y en el WROOM no se ha compilado: dilo siempre. No amplíes el límite de cuerpo de otras rutas (16 KB).
* La web puede servirse desde disco con `-w rpi/web` (solo desarrollo); tras cambiar `rpi/web/` regenera `web_pages.h` igualmente.
* En `tests/harness.cpp` el nodo 0 se reconfigura a los 3 s de arrancar (`setNodeConfig(0,2,1,128,255)`): las pruebas nuevas que usen ese arnés deben esperar a que pase. `SONOS_LFE_DC=1` pone señal en el canal LFE (solo `test_lfe.py`).
* No dejes procesos de depuración vivos (`pkill -f sonos_harness`): un arnés viejo ocupa el puerto 18080 y rompe la batería.
* Interfaz v9.13: pestañas Altavoces/Sonido/Latencia/Sistema; cajón del altavoz con «Ajustes avanzados» plegados (`#d_adv`; las pruebas lo abren con `adv()`), nombre por altavoz (`spkName(n)`, nunca «ESP n» fijo en HTML nuevo), menú «⋯» (`#more`/`#links`) en móvil vertical, tarjetas de estado `#hverd`, guía `#wiz`, y panel del cajón flotante en ≥1100×620 (`isDock()`, `body.has-dock`; no reduce la vista, sin `padding-right` en `main`; en móvil sigue siendo modal).
* Sistema/Raspberry (v9.15): `SystemManager` (`rpi/src/system_manager.cpp`): cambiar la WiFi de la RPi y actualizar el servidor solo con contraseña configurada (403 si no), sin ejecutar nunca texto de la web: la actualización solo lanza `sonos_wifi_update.service` (root, `scripts/update.sh`). Las pruebas usan `SONOS_NMCLI`, `SONOS_UPDATE_CMD` y `SONOS_UPDATE_DIR`. Sin probar en una Raspberry real.
* Interfaz v9.16: apariencia en Sistema → Interfaz (`#sp_ui`: `#ui_theme`, `#ui_shape`, `#ui_size`, `#ui_wiz`, `#ui_reset`) vía `window.SonosTheme` (`theme.js`: `set`, `shape`, `size`, `reset`); la cabecera del panel no tiene botón de tema. Barra del plano: `#lble` (solo «+»), `#lcal`, `#medit` («Editar plano») y la ayuda `#mhint` debajo.
