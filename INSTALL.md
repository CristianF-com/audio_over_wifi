# SonosWiFi — Guía de instalación por consola

Dos partes independientes:

1. **Raspberry Pi 400**: compilar e instalar el streamer (`sonos_wifi`) como servicio.
2. **ESP32-C5 (XIAO)**: compilar el firmware, grabarlo y darle su número y su WiFi.

> **Estado de verificación.** El código de la Raspberry se compila y se prueba con la batería de tests (`tests/run_tests.sh`).
> El firmware del ESP32 **no se ha compilado con un ESP-IDF real ni probado en placas**: si el primer `idf.py build`
> falla, copia el error completo y se corrige. Lo mismo para el Bluetooth desde la Raspberry (solo probado simulado).

---

## 0. Qué necesitas

| Para | Qué |
|---|---|
| Raspberry Pi 400 | Raspberry Pi OS de 64 bits (Bookworm o posterior), conectada por **Ethernet** al router, con acceso a internet para instalar paquetes |
| Interfaz de audio | Traktor Audio 10 por USB (ALSA, 10 canales, 48 kHz) |
| Router | Wi-Fi 6 en **5 GHz**, dedicado al audio si es posible |
| Cada receptor | XIAO ESP32-C5 + PCM5102A (cableado en el [README](README.md#hardware-y-cableado-por-nodo)) y un cable USB-C de **datos** |
| Computadora para flashear | Linux o macOS con ESP-IDF 5.5 o superior (en Windows, usa WSL2 o el «ESP-IDF PowerShell») |

---

## 1. Raspberry Pi 400

Todos los comandos se ejecutan en la Raspberry, por SSH o en su terminal.

### 1.1 Preparar el sistema

```bash
sudo apt update && sudo apt full-upgrade -y
sudo apt install -y git
```

### 1.2 Descargar el código

```bash
git clone https://github.com/CristianF-com/audio_over_wifi.git
cd audio_over_wifi
```

### 1.3 Instalar con un solo comando (recomendado)

```bash
sudo ./scripts/build.sh
```

Ejecútalo con tu usuario normal y `sudo` (no como `root` directo): el servicio corre con ese usuario.

El script hace, en orden:

1. Instala las dependencias: `libasound2-dev libssl-dev openssl cmake g++ pkg-config python3 bluez` y `python3-bleak` (o `bleak` por `pip` si el paquete no existe).
2. Compila en `rpi/build/` (Release) e instala `/usr/local/bin/sonos_wifi`.
3. Copia el ayudante Bluetooth a `/usr/local/lib/sonos_wifi/sonos_ble_prov.py` y añade tu usuario al grupo `bluetooth`.
4. Genera un certificado HTTPS autofirmado en `/etc/sonos_wifi/` (hace falta para el micrófono y Web Bluetooth del navegador).
5. Crea y arranca el servicio `sonos_wifi` en systemd, con prioridad de tiempo real (FIFO 50 para todo el proceso desde systemd; el programa sube a 80 el hilo de captura y a 60 el de control).

Al terminar muestra las direcciones web. Si el script te añadió al grupo `audio` por primera vez, **cierra la sesión y vuelve a entrar** (o reinicia).

### 1.4 Compilar a mano (sin instalar el servicio)

Útil para probar o depurar:

```bash
sudo apt install -y libasound2-dev libssl-dev openssl cmake g++ pkg-config
cd rpi
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"
./build/sonos_wifi -d auto -p 8080
```

Opciones principales:

| Opción | Significado |
|---|---|
| `-d auto` | busca la tarjeta cuyo nombre contiene «traktor» (el índice `hw:N` cambia entre reinicios). Para fijar una: `-d hw:2,0` |
| `-p 8080` | puerto de la web por HTTP |
| `-S 8443` | puerto HTTPS (por defecto 8443; `0` lo desactiva). Usa el certificado de `/etc/sonos_wifi/` o el que indiques con `-C cert.pem -K key.pem`; si no existe, no hay HTTPS |
| `-P 240` | frames por período ALSA |
| `-c fichero` | archivo de estado persistente (volúmenes, canales, latencias, EQ) |
| `-W fichero` | archivo con la contraseña de la web |
| `-B ayudante.py` | ruta del ayudante Bluetooth |

### 1.5 Comprobar que funciona

```bash
sudo systemctl status sonos_wifi          # debe decir "active (running)"
journalctl -u sonos_wifi -f               # registro en vivo (Ctrl+C para salir)
arecord -l | grep -i traktor              # la Traktor debe aparecer
curl -s http://localhost:8080/api/status  # JSON con "running":true cuando captura
```

Abre desde el móvil o la PC, en la misma red:

| Página | Dirección |
|---|---|
| Panel principal | `http://<ip-de-la-rpi>:8080` |
| Calibración (micrófono del móvil) | `https://<ip-de-la-rpi>:8443/calibrate` |
| Añadir altavoces por Bluetooth desde el móvil | `https://<ip-de-la-rpi>:8443/ble` |

En las dos páginas HTTPS el navegador avisará del certificado autofirmado: acéptalo.
Para saber la IP: `hostname -I`.

### 1.6 Operación diaria

```bash
sudo systemctl restart sonos_wifi    # reiniciar
sudo systemctl stop sonos_wifi       # parar (por ejemplo para usar la Traktor con otra cosa)
sudo systemctl disable sonos_wifi    # que no arranque con el sistema
```

**Cambiar parámetros del servicio** (otro puerto, otra tarjeta, etc.): edita la línea `ExecStart` y recarga.

```bash
sudo systemctl edit --full sonos_wifi
sudo systemctl daemon-reload && sudo systemctl restart sonos_wifi
```

**Contraseña para la web** (úsala con HTTPS, por HTTP viaja sin cifrar):

```bash
sudo ./scripts/set_password.sh "mi clave"    # mínimo 6 caracteres
sudo ./scripts/set_password.sh --off         # quitarla
```

### 1.7 Actualizar y desinstalar

```bash
cd ~/audio_over_wifi && git pull && sudo ./scripts/build.sh    # actualizar (conserva el estado guardado)

sudo systemctl disable --now sonos_wifi                         # desinstalar
sudo rm -f /etc/systemd/system/sonos_wifi.service /usr/local/bin/sonos_wifi
sudo rm -rf /usr/local/lib/sonos_wifi /etc/sonos_wifi /var/lib/sonos_wifi
sudo systemctl daemon-reload
```

### 1.8 Problemas frecuentes

| Síntoma | Causa y solución |
|---|---|
| `Device busy` / no abre la tarjeta | PipeWire o PulseAudio la ocupa: `systemctl --user stop pipewire pipewire-pulse wireplumber`, o desactiva ese dispositivo en WirePlumber |
| No aparece la Traktor en `arecord -l` | Cable USB o puerto: prueba otro. El servicio reintenta cada 3 s, no hace falta reiniciarlo |
| La web de `:8443` no carga | El certificado se crea una sola vez; si cambió la IP, bórralo (`sudo rm /etc/sonos_wifi/*.pem`) y vuelve a ejecutar `build.sh` |
| El botón «Añadir altavoces» dice que falta el ayudante Bluetooth | Instala `sudo apt install python3-bleak` y reinicia el servicio. Si no, usa la página `/ble` desde el móvil |
| El Bluetooth de la Raspberry falla | `sudo systemctl status bluetooth`, `rfkill list`, y comprueba que tu usuario está en el grupo `bluetooth` (`groups`) |

---

## 2. ESP32-C5 (XIAO)

Todos los comandos se ejecutan en la computadora con la que flasheas, no en la Raspberry (aunque también puedes hacerlo desde la propia Raspberry si instalas ESP-IDF ahí).

### 2.1 Instalar ESP-IDF 5.5 o superior (una sola vez)

Linux (Debian/Ubuntu):

```bash
sudo apt install -y git wget flex bison gperf python3 python3-pip python3-venv cmake ninja-build ccache libffi-dev libssl-dev dfu-util libusb-1.0-0
mkdir -p ~/esp && cd ~/esp
git clone --recursive -b v5.5 https://github.com/espressif/esp-idf.git
cd esp-idf && ./install.sh esp32c5
```

macOS: instala antes las herramientas con `brew install cmake ninja dfu-util python3`, y sigue desde `git clone`.

En cada terminal nueva, activa el entorno antes de usar `idf.py`:

```bash
. ~/esp/esp-idf/export.sh
```

> Los scripts del proyecto esperan `$IDF_PATH` definido (lo hace `export.sh`). Si instalaste ESP-IDF en otra ruta, solo cambia el `.` de arriba.

### 2.2 Permisos y puerto serie

Linux: añade tu usuario al grupo `dialout` y cierra sesión y vuelve a entrar.

```bash
sudo usermod -aG dialout "$USER"
```

Conecta la placa por USB-C y averigua el puerto:

```bash
ls /dev/ttyACM* /dev/ttyUSB* 2>/dev/null     # Linux (normalmente /dev/ttyACM0)
ls /dev/cu.usbmodem*                         # macOS
```

Si `idf.py flash` no logra conectar, pon la placa en modo bootloader: **mantén pulsado BOOT, conecta el USB (o pulsa RESET) y suelta BOOT**, y repite.

### 2.3 Compilar

```bash
cd ~/audio_over_wifi/esp32
idf.py set-target esp32c5
idf.py build
```

`set-target` regenera la configuración desde `sdkconfig.defaults`; ejecútalo la primera vez o si cambias de objetivo (borra el `sdkconfig` actual).

Si ya habías compilado antes y actualizas `sdkconfig.defaults` (por ejemplo para el tamaño de la partición de la app), repite `idf.py set-target esp32c5` o borra `esp32/sdkconfig`; si no, se sigue usando la configuración vieja. El binario supera 1 MB, por eso `sdkconfig.defaults` usa la tabla de particiones de aplicación grande (1,5 MB).

Valores por defecto opcionales (los puedes dejar y configurar después por NVS o Bluetooth):

```bash
idf.py menuconfig        # menú «SonosWiFi receiver»
```

Ahí se cambia: ID por defecto, SSID y contraseña por defecto, banda (solo 5 GHz, automática o solo 2,4 GHz), pines I2S (BCK=8, WS=9, DOUT=10), IP fija de la Raspberry y el Bluetooth de configuración.

### 2.4 Grabar el firmware

```bash
idf.py -p /dev/ttyACM0 flash
```

El **mismo binario** sirve para todas las placas: lo que las distingue es su número (ID) y su WiFi, que se guardan aparte en la memoria NVS.

### 2.5 Darle número y WiFi (elige una opción)

**A. Por consola, con el script (sin recompilar)**

```bash
cd ~/audio_over_wifi
./scripts/provision_esp.sh <id 0-9> "<nombre-de-la-wifi>" "<contraseña>" /dev/ttyACM0
# ejemplo:
./scripts/provision_esp.sh 0 "MiRed5G" "clave-larga" /dev/ttyACM0
```

Escribe el ID y la WiFi directamente en la partición NVS (0x9000). Necesita el entorno ESP-IDF activo.

**B. Por Bluetooth, sin cable**

Con la placa encendida aparece como `SonosWiFi-XXXX`. Se configura desde el botón **Añadir altavoces** del panel de la Raspberry, o desde el móvil en `https://<ip-de-la-rpi>:8443/ble`. Detalles en el [README](README.md#configurar-los-esp32-por-bluetooth-sin-cable-sin-wi-fi).

**C. En el propio firmware**

`idf.py menuconfig` → *SonosWiFi receiver* → ID, SSID y contraseña por defecto, y vuelve a compilar y grabar. Solo se usan si la NVS está vacía.

### 2.6 Comprobar que funciona

```bash
idf.py -p /dev/ttyACM0 monitor        # Ctrl+] para salir
```

En el registro deberías ver, en orden:

1. `=== SonosWiFi Receiver (protocolo v5) ===` y `ID=<n> ch=…`
2. `Conectando a WiFi '<red>'...`, luego `AP canal … (5 GHz) RSSI …` y `IP: <dirección>`
3. `I2S OK …` y `Sockets OK | control :5005 | audio :5006`
4. `Listo. Esperando WELCOME de la RPi...` y después `WELCOME: …` cuando la Raspberry lo reconoce

Si el canal sale por debajo de 36, está en 2,4 GHz: revisa la banda en `menuconfig` y que tu router emita en 5 GHz. En el panel de la Raspberry, el altavoz aparece en línea.

### 2.7 Varias placas

Repite 2.4 y 2.5 por cada placa, **cambiando el ID** (0, 1, 2, … hasta 9) y el puerto si conectas varias a la vez. Mientras la Raspberry esté en funcionamiento, los canales se reasignan en vivo desde el panel.

| ID | Canales (distribución de 5 estéreo) | Uso típico |
|---|---|---|
| 0 | 0, 1 | Frontal izq./der. |
| 1 | 2, 3 | Central/LFE |
| 2 | 4, 5 | Envolvente izq./der. |
| 3 | 6, 7 | Traseros izq./der. |
| 4 | 8, 9 | Extra |

En modo «10 mono», cada ID (0–9) es un altavoz con su propio canal.

### 2.8 Actualizar o borrar

```bash
cd ~/audio_over_wifi/esp32
idf.py -p /dev/ttyACM0 build flash   # actualiza el firmware; el ID y la WiFi (NVS) se conservan
idf.py -p /dev/ttyACM0 erase-flash   # borra TODO, incluido ID y WiFi; hay que grabar y provisionar de nuevo
```

> **Si cambia la versión del protocolo** (`common/protocol.h`, `PROTOCOL_VERSION`), hay que grabar el firmware nuevo en **todas** las placas a la vez que se actualiza la Raspberry. Si no, las placas no se conectan.

### 2.9 Problemas frecuentes

| Síntoma | Causa y solución |
|---|---|
| `idf.py: command not found` | Falta activar el entorno: `. ~/esp/esp-idf/export.sh` |
| `Permission denied: /dev/ttyACM0` | Falta el grupo `dialout` (2.2) y volver a iniciar sesión |
| No detecta la placa / `Failed to connect` | Cable solo de carga (usa uno de datos), o falta el modo bootloader (2.2) |
| Error de compilación | Copia el error completo: el firmware no se ha probado con un ESP-IDF real |
| Conecta pero no suena | Mira el monitor: ¿llega el `WELCOME`? Si no, mira que estén en la misma red que la Raspberry y que el router no aísle clientes. Si es así, pon la IP de la Raspberry (menuconfig, Bluetooth o panel) |
| Cortes de audio | En el panel, pestaña Latencia: sube el colchón de red y mira Perdidos/Tarde en la tabla |

---

## 3. Orden recomendado de puesta en marcha

1. Instalar la Raspberry (sección 1) y comprobar que la web carga y que se ve la Traktor.
2. Grabar y provisionar **un** ESP32 (sección 2) y verlo en línea en el panel.
3. Verificar que suena (pestaña Sonido: pass-through por defecto) y repetir con el resto de las placas.
4. Calibrar con el micrófono del móvil (`/calibrate`) y ajustar el retardo de audio en el reproductor de vídeo (pestaña Latencia).
