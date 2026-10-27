# SonosWiFi — Audio 7.1 inalámbrico (Traktor Audio 10 → RPi 400 → 5× XIAO ESP32-C5 + PCM5102A)

```
Traktor Audio 10 (USB, S32_LE 10ch 48k)
        │ ALSA, hilo RT
        ▼
 Raspberry Pi 400 [sonos_wifi]  ── Ethernet al router (recomendado)
        │  UDP unicast :5006 (audio, 500 B/paquete, 2.5 ms)   UDP :5005 (control/sync)
        ▼  WiFi (5 GHz recomendado)
 ESP32-C5 #0..#4  ──I2S──►  PCM5102A  ──►  amplificador/altavoz
 ch 0+1  2+3  4+5  6+7  8+9
```

La RPi marca cada paquete con su hora de captura; cada ESP sincroniza su reloj con la RPi
(ping/pong tipo NTP) y reproduce a `captura + 80 ms`. Así todos suenan alineados aunque
la red entregue los paquetes en momentos distintos. El volumen se aplica solo en la RPi.

## Hardware y cableado (por nodo)

| XIAO ESP32-C5 | GPIO | PCM5102A |
|---|---|---|
| D8  | 8  | BCK |
| D9  | 9  | LCK (WS) |
| D10 | 10 | DIN |
| 3V3 (o 5V según módulo) | | VIN |
| GND | | GND **y SCK** |

Además en el PCM5102A: `SCK→GND` (usa su PLL interno, sin MCLK), `FMT→GND`, `XMT→3V3` (unmute),
`FLT→GND`, `DEMP→GND`. Pines cambiables en `idf.py menuconfig → SonosWiFi receiver`.
Evita D2/D3 (GPIO25/GPIO7, strapping). **Verifica el pinout contra el esquemático de tu placa.**

## RPi 400

```bash
sudo ./scripts/build.sh          # compila, instala /usr/local/bin/sonos_wifi y el servicio systemd
journalctl -u sonos_wifi -f
```
Web UI: `http://<ip-rpi>:8080` · busca la tarjeta Traktor sola (`-d auto`); o `-d hw:2,0`.
Opciones: `-d <dev|auto> -p <puerto web> -P <frames por período ALSA, def. 240>`.

Recomendado: RPi por **Ethernet**; ESP en **5 GHz** (SSID de 5 GHz o router con band steering).
Si PipeWire/PulseAudio ocupa la tarjeta ("Device busy"), desactiva ese dispositivo en WirePlumber
o usa `systemctl --user stop pipewire pipewire-pulse wireplumber`.

## ESP32-C5 (ESP-IDF ≥ 5.5)

```bash
. ~/esp-idf/export.sh
cd esp32
idf.py set-target esp32c5
idf.py build
idf.py -p /dev/ttyACM0 flash                 # el MISMO binario sirve para las 5 placas
../scripts/provision_esp.sh 0 "MiRed5G" "clave" /dev/ttyACM0   # ID 0 + WiFi en NVS
idf.py -p /dev/ttyACM0 monitor
```
Repite `flash` + `provision_esp.sh` con ID 1, 2, 3… en cada placa (hasta ID 9).
(Alternativa sin NVS: `idf.py menuconfig` → SonosWiFi receiver → ID/SSID/clave por defecto.)

| ID | Canales | Uso |
|---|---|---|
| 0 | 0,1 | Front L/R |
| 1 | 2,3 | Center/LFE |
| 2 | 4,5 | Surround L/R |
| 3 | 6,7 | Rear L/R |
| 4 | 8,9 | Aux |

Reasignable en vivo desde la web UI.

## Latencia ajustable, ocultación de pérdidas y estado persistente (v6)

* **Latencia en caliente:** tarjeta «Latencia» del panel (40 / 60 / 80 / 120 ms o deslizador 40–200 ms). Se envía a todos los ESP
  sin reiniciar; al cambiar hay un breve hueco (si la subes) o un pequeño salto (si la bajas) mientras reconvergen.
  Más baja = menos retardo audio↔imagen pero más riesgo de cortes: vigila «tarde» en cada nodo. Para películas, pon en el reproductor
  un desfase de audio de −(valor) ms. Por defecto 80 ms (validado por tu prueba previa en 2.4 GHz, que usaba ≥85 ms de colchón).
* **Ocultación de pérdidas (PLC):** si se pierde algún paquete (hasta 8 seguidos), el ESP rellena el hueco con el último audio
  en fade-out y entra con fade-in. Mantiene la línea de tiempo (el altavoz no se desincroniza) y evita clics.
  En simulación con 1 % de pérdidas, sin PLC el error de alineación se iba a −5 ms; con PLC se queda en ±0,1 ms.
* **Persistencia:** volúmenes, canales, habilitados, ajustes de latencia por nodo, volumen maestro y latencia global se guardan
  en `/var/lib/sonos_wifi/config.txt` (texto `clave=valor`, escritura atómica) y se recargan al arrancar.
* **Contraseña opcional:** `sudo ./scripts/set_password.sh "mi clave"` (HTTP Basic, usuario libre; `--off` la quita). Úsala por HTTPS (:8443).
* **IP fija de la RPi (opcional):** campo en la página `/ble` (o `menuconfig`). Los ESP envían además un HELLO unicast a esa IP,
  útil si el router filtra el broadcast (aislamiento de clientes). Vacío = descubrimiento automático.
* **Protocolo v3→v4:** hay que **reflashear todos los ESP** (la RPi ignora los que hablen otra versión y lo indica en el log).

## Un ESP por canal (hasta 10 receptores) — v7

Además del modo clásico (5 ESP estéreo, cada uno con un par de canales) puedes usar **un ESP32 + DAC por altavoz** (hasta 10).

* **Cada ESP32 elige su salida** en el panel (tarjeta del nodo → *Salida de este ESP32*): **Estéreo** (canal izq. + canal der.) o **Mono** (un canal, sale por L y R del PCM5102A). Se pueden mezclar: p. ej. 2 ESP estéreo para frontales y 6 mono para el resto.
* Botones de atajo en *Distribución de altavoces*: **5 ESP estéreo** (nodo *i* = canales 2i, 2i+1; nodos 5–9 sin usar) y **10 ESP mono** (nodo *i* = canal *i*). Los atajos conservan volúmenes y ajustes de latencia; al editar un nodo a mano la distribución pasa a «Personalizada».
* Asignación de IDs: ID 0–9 por placa (BLE `/ble`, `provision_esp.sh <id 0-9>` o menuconfig). Con 10 mono, canal 0 = FL, 1 = FR, 2 = C, 3 = LFE, 4 = SL, 5 = SR, 6 = SBL, 7 = SBR, 8/9 = AUX.
* **Tráfico:** los nodos mono reciben paquetes `PKT_AUDIO_MONO` (240 frames mono de 5 ms, mismos 500 B), así que 10 ESP mono generan los mismos ~2000 paquetes/s que 5 estéreo.
* **Protocolo v4:** hay que reflashear todos los ESP (la RPi ignora los de versiones anteriores y lo indica en el log). Calibración: con 10 nodos y 3 rondas dura ~47 s.
* API: `POST /api/layout {"layout":"stereo5"|"mono10"}`; `POST /api/node` acepta `"mono":true|false`.

## Pérdidas, graves, ecualización y lip-sync (v8, protocolo v5)

* **Reenvío de paquetes perdidos (NACK).** El ESP32 detecta huecos en la secuencia, pide a la RPi que reenvíe (hasta 8 paquetes por petición, con un segundo intento a mitad de espera) y reordena. Espera como máximo ¼ del colchón de reproducción (80 ms → 20 ms; 40 ms → 10 ms): lo que no llega en ese plazo lo oculta el PLC. Con la red limpia no cuesta ni un byte. La RPi guarda los últimos 128 paquetes por nodo (≈0.3 s en estéreo, 0.6 s en mono) y limita los reenvíos a 300/s por nodo. En el panel, `perd` = definitivamente perdidos, `rec` = recuperados. Con esto puedes bajar el colchón con más seguridad.
* **Gestión de graves.** Cada altavoz tiene un *rol*: **Rango completo**, **Satélite** (pasa-altos) o **Subwoofer** (pasa-bajos). Con un sub online, los satélites pierden los graves y el sub recibe su canal (LFE) **más** los graves de todos los satélites, con crossover Linkwitz-Riley de 4.º orden (24 dB/oct, suma plana en módulo y fase) a la frecuencia que elijas (40–200 Hz, 80 Hz por defecto). Si el sub se apaga o se desconecta, los satélites vuelven solos a rango completo. Atajo: tarjeta *Gestión de graves* → elige el nodo del sub → *Aplicar* (el resto de altavoces habilitados pasan a satélite). Se procesa en la RPi (los ESP no tocan las muestras) y se desactiva durante la calibración.
* **Ecualizador por altavoz.** 6 bandas de 1 octava (354, 707, 1.4 k, 2.8 k, 5.7 k y 9.8 kHz), ±12 dB, editables en la tarjeta de cada nodo (*EQ del altavoz*). La calibración con el móvil lo propone sola: mide la respuesta por bandas de cada altavoz y los **iguala al promedio del conjunto** (el mismo micrófono mide a todos, así que su propia respuesta se cancela; el nivel global lo corrige el volumen; máx. ±6 dB). Los subwoofers vienen desmarcados (columnas *vol* y *eq*). Botones: *Aplicar EQ* / *Aplicar todo* / *Restablecer*. Ojo: un EQ con refuerzos baja el margen antes de saturar; si ves recorte, baja el volumen de ese nodo.
* **Lip-sync automático.** El audio llega a los altavoces `playout` ms después que la imagen (+ el *ajuste extra* del panel, p. ej. el retardo de la entrada USB). El panel muestra el desfase a poner en el reproductor, y `GET /api/latency` lo da como JSON (`mpv_audio_delay_s`). Para **mpv** copia `scripts/sonos_lipsync.lua` a `~/.config/mpv/scripts/` (Windows: `%APPDATA%\mpv\scripts\`) y añade a `mpv.conf`: `script-opts=sonos_lipsync-host=192.168.1.10,sonos_lipsync-port=8080` (más `sonos_lipsync-user/-pass` si pusiste contraseña). Se actualiza al abrir cada vídeo y cada 10 s, así que si cambias la latencia en el panel el reproductor la sigue. Otros reproductores: usa el valor del panel a mano (VLC: Herramientas → Sincronización de pista; Kodi: Ajuste de retardo de audio).
* **Periodo de captura.** `-P <frames>` baja la latencia de captura (240 por defecto = 5 ms; 120 = 2.5 ms, algo más de CPU).
* **Router dedicado (recomendado, ya lo usas).** Canal fijo de 5 GHz sin DFS (36–48 ó 149–161), ancho 20–40 MHz (con tráfico pequeño y constante es más estable que 80), sin *Smart Connect*/band steering, WMM activo, DTIM = 1, sin clientes ajenos y los ESP a la vista del router. ESP-NOW quedó descartado (no da abasto con tantos canales).
* **Protocolo v5:** hay que reflashear todos los ESP.

## Interfaz web y pass-through (v9)
Las tres páginas se rediseñaron (negro + azul `#4aa3ff`). Las fuentes están en `rpi/web/` (`panel.html`, `calibrate.html`, `ble.html`, `shared.css`, `analysis.js`); `python3 scripts/gen_web_pages.py` genera `rpi/include/web_pages.h` (no editar a mano; los tests comprueban que está al día).
* **Panel (`/`)**: plano de la sala con cada altavoz en la posición de su canal; al tocarlo se abre un cajón con salida estéreo/mono, canales, volumen, función en graves, retardo y EQ, con **autoguardado**. Pestañas Altavoces · Sonido · Latencia · Registro.
* **Pass-through (pestaña Sonido)**: por defecto el crossover y la EQ están **apagados**: el audio sale tal cual, solo con volumen y retardo por altavoz. Úsalo cuando la Traktor ya envía los canales separados. Con una señal estéreo o mezclada, elige **Procesado** (activa crossover y EQ; cada uno se puede apagar aparte) y escoge el subwoofer. API: `POST /api/dsp {"xover":bool,"eq":bool}`; el estado está en `/api/status` (`dsp_xover`, `dsp_eq`).
* **Cambio respecto a v8**: en v8 el procesado estaba activo siempre que hubiera roles/EQ; ahora hay que activarlo (se guarda en la configuración).
* **/ble**: pasos guiados, número 0–9 en botones, estado legible (WiFi, señal, IP, Raspberry). **/calibrate**: asistente en 3 pasos, tarjeta por altavoz con barras de EQ, qué aplicar, y ofrece activar la EQ si estaba en pass-through.
* Pruebas: `tests/run_tests.sh` (batería completa) y `tests/run_ui_tests.sh [carpeta]` (Playwright: autoguardado, pass-through, Web Bluetooth simulado, calibración con resultados inyectados, móvil sin scroll horizontal).

## Configurar los ESP32 por Bluetooth desde la propia Raspberry (v9.1)
En el panel, **Añadir altavoces**: guardas una vez la WiFi de los altavoces (SSID y contraseña; la Raspberry las guarda en `config.txt.ble` con permisos 0600 y la API nunca devuelve la contraseña), pulsas **Buscar altavoces por Bluetooth** y aparece la lista de ESP32 `SonosWiFi-XXXX` cercanos con su estado (nuevo/sin WiFi, o ya en la red con su número). Cada uno trae un número sugerido (el propio si ya estaba en la red; si no, el menor libre que no use un altavoz en línea). Botones **Sonar** (pitido para saber cuál es), **Configurar** (por altavoz) y **Configurar los N nuevos** (con confirmación que lista a cuál se envía qué). Siempre bajo demanda: nunca configura nada sin que lo pidas.
* **Cómo funciona:** el servidor lanza `scripts/sonos_ble_prov.py` (BlueZ + `bleak`) como subproceso; las órdenes (con la contraseña) van por stdin, no por argumentos. El firmware no cambia: usa el mismo servicio GATT que la página `/ble`. Escribe ID → SSID → IP de la RPi (si hay) → contraseña → `apply`.
* **Instalación:** `scripts/build.sh` instala `python3-bleak` (o `pip`), copia el ayudante a `/usr/local/lib/sonos_wifi/` y añade el usuario del servicio al grupo `bluetooth`. Ruta alternativa: `-B /ruta/sonos_ble_prov.py`. Si no hay ayudante, el panel lo dice y ofrece la página `/ble` del móvil.
* **API:** `GET /api/ble/status`, `POST /api/ble/wifi {ssid,pass,rpi_ip}`, `POST /api/ble/scan {seconds 3-15}`, `POST /api/ble/provision {address,id,force}`, `POST /api/ble/identify {address}`. Una sola operación Bluetooth a la vez.
* **Notas:** el firmware exige enlace cifrado (Just Works) para escribir (`SONOS_BLE_REQUIRE_ENC`); el ayudante intenta emparejar antes. Un ESP32 con WiFi ya funcionando cierra su BLE a los 300 s del arranque: para reconfigurarlo, reinícialo. Alcance BLE: unos metros.
* **Pruebas:** `tests/test_ble_helper.py` (bleak falso: qué se escribe y en qué orden), la API contra el ayudante en modo simulado (`SONOS_BLE_MOCK`) en `fake_esp.py`, y la interfaz en `test_ui.js`. **No probado con Bluetooth real ni con ESP32 reales.**

## Wi-Fi 6 en 5 GHz (configuración recomendada)

El firmware viene fijado a **solo 5 GHz** (`menuconfig → SonosWiFi receiver → Banda WiFi`; también «automática» o «solo 2.4»).
Se hace con `esp_wifi_set_band_mode()` justo antes del primer `connect()`. Motivo: el C5 no usa las dos bandas a la vez y,
si el router comparte SSID entre 2.4 y 5 GHz, el escaneo por señal tiende a elegir 2.4 GHz.
Consejos de router (generales, no medidos en tu red):
* Canal **36–48** (sin DFS): evita que un radar fuerce un cambio de canal y corte el audio un instante.
* Si puedes, desactiva *Smart Connect*/band steering para este SSID, o usa un SSID de 5 GHz exclusivo.
* WPA2 o WPA3 valen (el firmware negocia ambos). La RPi por **Ethernet**.
* Verifica la banda real: el log del ESP y el estado BLE muestran el canal (`AP canal 36 (5 GHz)`; `"ch":36`).
  Si la placa no encuentra ningún AP de 5 GHz no se conecta (en ese caso cambia a «automática»).
* El C5 usa HT40 por defecto en 5 GHz; si ves pérdidas con un canal ocupado, un ancho de 20 MHz en el router es más robusto.

## Configurar los ESP32 por Bluetooth (sin cable, sin Wi-Fi)

Cada receptor se anuncia por BLE como **`SonosWiFi-XXXX`** (XXXX = final de su MAC, único aunque todos tengan ID 0)
durante los primeros 5 min tras arrancar, y siempre que no tenga Wi-Fi.
Ahí puedes cambiar **ID, red Wi-Fi y contraseña**, ver su estado en vivo (IP, RSSI, sync, pérdidas) y hacer que
**pite** para saber qué placa es cuál. Después del primer arranque con Wi-Fi, el BLE se apaga solo (cero impacto en audio).

**Cómo acceder:**
1. **Android / PC (Chrome o Edge):** con el móvil en la misma red que la RPi, abre `https://<ip-rpi>:8443/ble`
   (acepta el aviso del certificado) → *Buscar y conectar* → elige `SonosWiFi-XXXX` → rellena ID/SSID/clave → *Guardar y reiniciar*.
   Si el sistema pide emparejar, acepta (es un emparejamiento «Just Works»; el enlace va cifrado).
2. **iPhone:** Safari no soporta Web Bluetooth. Usa la app gratuita **Bluefy** y abre la misma URL.
3. **Sin la RPi:** cualquier app BLE genérica (**nRF Connect**): servicio `5c0a0001-8e3b-4d5e-9a41-534f4e4f5346`;
   características `…0002` SSID (texto), `…0003` clave (texto, 8–63), `…0004` ID (1 byte), `…0005` estado (JSON, lectura),
   `…0006` comando (texto `apply` / `identify` / `reboot` / `factory`), `…0007` IP de la RPi (texto, opcional). Escribe SSID/clave/ID y por último `apply`.
4. Quitar BLE por completo: `idf.py menuconfig → SonosWiFi receiver → desmarcar «Configuración por Bluetooth LE»`.

## Calibración con el micrófono del móvil

`https://<ip-rpi>:8443/calibrate` (la primera vez, acepta el certificado y concede el micrófono).
Coloca el móvil **en el punto de escucha** (hacia ahí queda centrado el audio), sala en silencio, y pulsa *Iniciar*.
Cada nodo emite un barrido de 0,3 s, uno tras otro (2 rondas, ~20 s); la página calcula cuándo y con qué nivel llega cada uno y propone:
* **Latencia por nodo** (resolución de µs): retrasa los más cercanos para que todos lleguen a la vez.
* **Volumen por nodo**: iguala al más bajo. Desmarca «vol» en subwoofers o nodos que no sean de rango completo (el barrido es 250 Hz–12 kHz).
Pulsa *Aplicar*. Se puede repetir; los valores propuestos son absolutos (el test ignora los ajustes actuales).
La Traktor debe estar conectada (el reloj de la RPi sale de la captura). En iPhone el micrófono aplica AGC y los niveles son menos fiables (los retardos no).

## Calibración y diagnóstico

Log normal del ESP (cada 2 s):
`ID=0 ch=0/1 q=40% rx=… lost=0 late=0 under=0 jit=300us err=±100us rtt=3000us drift=±60ppm sync=1`

* `err` (sync_err) debe quedar en ±100 µs; `late`/`under` deben ser 0 en régimen.
* `late` crece → WiFi congestionado o `PLAYOUT_DELAY_US` insuficiente (en `common/protocol.h`, subir a 120 ms).
* `rtt` alto/errático → mala señal; el sync usa el mejor RTT de las últimas 8 muestras.
* Alineación fina: mide con micrófono y ajusta "Ajuste de latencia (ms)" por nodo en la web.
  Retraso fino por debajo de 1 ms: ajustar `DMA_LAT_US`/offset por nodo (no es parte de este código).

## Pruebas (sin hardware)

```bash
tests/run_tests.sh        # firmware (unidades, reordenación/NACK, tarea RX real en el PC), DSP, persistencia, autenticación, protocolo, canales, reenvío, graves/EQ/roles, lip-sync (script mpv), API web, HTTPS, calibración y análisis en Node
tests/check_pages_js.sh   # sintaxis del JavaScript de las páginas
python3 tests/sim_sync.py # controlador de sincronía: ±150 ppm, pérdidas con/sin PLC, cambio de latencia en caliente
```
El firmware ESP **no** se ha compilado con ESP-IDF real (solo chequeo de sintaxis con stubs). Ver PROYECTO_RESUMEN.md.

## Seguridad
La web UI/API no tiene autenticación ni CORS: úsala solo en una LAN de confianza. El HTTPS usa un certificado
autofirmado (`/etc/sonos_wifi/`), sirve para habilitar micrófono/Bluetooth del navegador, no para autenticar.
BLE: «Just Works» cifra el enlace pero no impide un atacante activo cercano durante la ventana de configuración;
por eso el BLE solo está activo 5 min tras arrancar (o sin Wi-Fi) y la contraseña Wi-Fi no es legible por BLE.
